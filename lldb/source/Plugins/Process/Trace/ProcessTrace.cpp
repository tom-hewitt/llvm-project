//===-- ProcessTrace.cpp --------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ProcessTrace.h"

#include <memory>
#include <optional>

#include "ThreadTrace.h"
#include "forward-declarations.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Section.h"
#include "lldb/Host/ThreadLauncher.h"
#include "lldb/Target/ABI.h"
#include "lldb/Target/SectionLoadList.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Event.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Target/StopInfo.h"
#include "llvm/Support/Error.h"

using namespace llvm;
using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE(ProcessTrace)

llvm::StringRef ProcessTrace::GetPluginDescriptionStatic() {
  return "Trace process plug-in.";
}

void ProcessTrace::Terminate() {
  PluginManager::UnregisterPlugin(ProcessTrace::CreateInstance);
}

ProcessSP ProcessTrace::CreateInstance(TargetSP target_sp,
                                       ListenerSP listener_sp,
                                       const FileSpec *crash_file,
                                       bool can_connect) {
  if (can_connect)
    return nullptr;
  return std::make_shared<ProcessTrace>(target_sp, listener_sp,
                                        crash_file ? *crash_file : FileSpec());
}

bool ProcessTrace::CanDebug(TargetSP target_sp, bool plugin_specified_by_name) {
  return plugin_specified_by_name;
}

ProcessTrace::ProcessTrace(TargetSP target_sp, ListenerSP listener_sp,
                           const FileSpec &core_file)
    : PostMortemProcess(target_sp, listener_sp, core_file),
      m_async_broadcaster(NULL, "lldb.process.trace.async_broadcaster") {
  m_async_broadcaster.SetEventName(eBroadcastBitAsyncThreadShouldExit,
                                   "async thread should exit");
  m_async_broadcaster.SetEventName(eBroadcastBitAsyncContinue,
                                   "async thread continue");
}

ProcessTrace::~ProcessTrace() {
  Clear();
  // We need to call finalize on the process before destroying ourselves to
  // make sure all of the broadcaster cleanup goes as planned. If we destruct
  // this class, then Process::~Process() might have problems trying to fully
  // destroy the broadcaster.
  Finalize(true /* destructing */);
}

void ProcessTrace::DidAttach(ArchSpec &process_arch) {
  ListenerSP listener_sp(
      Listener::MakeListener("lldb.process_trace.did_attach_listener"));
  HijackProcessEvents(listener_sp);

  SetCanJIT(false);
  StartPrivateStateThread(lldb::eStateStopped, false);
  if (!m_current_private_state_thread) {
    LLDB_LOG(GetLog(LLDBLog::Process), "ProcessTrace: failed to start private "
                                       "state thread.");
    return;
  }
  
  // We need to update the thread list to get the threads that are part of the
  // trace.
  UpdateThreadListIfNeeded();

  // "Run" in reverse back to the most recent instruction in each thread's trace.
  for (const auto &thread_sp : m_thread_list.Threads()) {
    ThreadTraceSP thread_trace_sp =
        std::static_pointer_cast<ThreadTrace>(thread_sp);
    Expected<TraceCursorSP> cursor_or_error = thread_trace_sp->GetTraceCursor();
    if (!cursor_or_error) {
      LLDB_LOG_ERROR(GetLog(LLDBLog::Process), cursor_or_error.takeError(),
                     "ProcessTrace::DidAttach failed to get trace cursor: {0}");
      continue;
    }
    TraceCursorSP cursor_sp = *cursor_or_error;

    cursor_sp->Seek(0, lldb::eTraceCursorSeekTypeEnd);
    cursor_sp->SetForwards(false);
    cursor_sp->Next();

    while (cursor_sp->HasValue()) {
      if (cursor_sp->GetItemKind() == lldb::eTraceItemKindInstruction) {
        // Use the last instruction in the trace for the thread's final state.
        thread_sp->GetRegisterContext()->SetPC(cursor_sp->GetLoadAddress());
        break;
      }
      cursor_sp->Next();
    }
  }

  // Pretend we stopped so we can show all of the threads
  // in the trace and explore the final state.
  SetPrivateState(lldb::eStateStopped);

  EventSP event_sp;
  WaitForProcessToStop(std::nullopt, &event_sp, true, listener_sp);

  RestoreProcessEvents();

  Process::DidAttach(process_arch);
}

bool ProcessTrace::DoUpdateThreadList(ThreadList &old_thread_list,
                                      ThreadList &new_thread_list) {
  return false;
}

void ProcessTrace::RefreshStateAfterStop() {}

Status ProcessTrace::DoResume(lldb::RunDirection direction) {
  // Only start the async thread if we try to do any process control.
  if (!m_async_thread.IsJoinable())
    StartAsyncThread();

  m_async_broadcaster.BroadcastEvent(
      eBroadcastBitAsyncContinue,
      std::make_shared<ProcessTraceEventData>(direction));

  return Status();
}

Status ProcessTrace::EnableBreakpointSite(BreakpointSite *bp_site) {
  return Status();
}

Status ProcessTrace::DisableBreakpointSite(BreakpointSite *bp_site) {
  return Status();
}

Status ProcessTrace::DoDestroy() { return Status(); }

size_t ProcessTrace::ReadMemory(addr_t addr, void *buf, size_t size,
                                Status &error) {
  if (const ABISP &abi = GetABI())
    addr = abi->FixAnyAddress(addr);

  // Don't allow the caching that lldb_private::Process::ReadMemory does since
  // we have it all cached in the trace files.
  return DoReadMemory(addr, buf, size, error);
}

void ProcessTrace::Clear() { m_thread_list.Clear(); }

void ProcessTrace::Initialize() {
  static llvm::once_flag g_once_flag;

  llvm::call_once(g_once_flag, []() {
    PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                  GetPluginDescriptionStatic(), CreateInstance);
  });
}

ArchSpec ProcessTrace::GetArchitecture() {
  return GetTarget().GetArchitecture();
}

bool ProcessTrace::GetProcessInfo(ProcessInstanceInfo &info) {
  info.Clear();
  info.SetProcessID(GetID());
  info.SetArchitecture(GetArchitecture());
  ModuleSP module_sp = GetTarget().GetExecutableModule();
  if (module_sp) {
    const bool add_exe_file_as_first_arg = false;
    info.SetExecutableFile(GetTarget().GetExecutableModule()->GetFileSpec(),
                           add_exe_file_as_first_arg);
  }
  return true;
}

size_t ProcessTrace::DoReadMemory(addr_t addr, void *buf, size_t size,
                                  Status &error) {
  Address resolved_address;
  GetTarget().ResolveLoadAddress(addr, resolved_address);

  return GetTarget().ReadMemoryFromFileCache(resolved_address, buf, size,
                                             error);
}

const ProcessTrace::ProcessTraceEventData *
ProcessTrace::ProcessTraceEventData::GetEventData(const Event *event_ptr) {
  if (!event_ptr)
    return nullptr;
  const EventData *event_data = event_ptr->GetData();
  if (!event_data || event_data->GetFlavor() != GetFlavorString())
    return nullptr;
  return static_cast<const ProcessTraceEventData *>(event_data);
}

bool ProcessTrace::StartAsyncThread() {
  if (m_async_thread.IsJoinable()) {
    LLDB_LOGF(GetLog(LLDBLog::Process),
              "ProcessTrace::StartAsyncThread called but "
              "async thread is already running.");

    return false;
  }

  m_async_listener_sp = Listener::MakeListener("ProcessTrace::AsyncThread");
  if (m_async_listener_sp->StartListeningForEvents(
          &m_async_broadcaster,
          eBroadcastBitAsyncContinue | eBroadcastBitAsyncThreadShouldExit) ==
      0) {
    LLDB_LOGF(GetLog(LLDBLog::Process),
              "ProcessTrace::StartAsyncThread failed to start listening for "
              "events.");
    return false;
  }

  llvm::Expected<HostThread> async_thread = ThreadLauncher::LaunchThread(
      "<lldb.process.trace.async>", [this] { return AsyncThread(); });
  if (!async_thread) {
    LLDB_LOG_ERROR(GetLog(LLDBLog::Host), async_thread.takeError(),
                   "failed to launch host thread: {0}");
    return false;
  }

  m_async_thread = *async_thread;

  return true;
}

thread_result_t ProcessTrace::AsyncThread() {
  Log *log = GetLog(LLDBLog::Process);
  LLDB_LOG(log, "ProcessTrace::%s(pid = %" PRIu64 ") thread starting...",
           __FUNCTION__, GetID());

  EventSP event_sp;
  bool done = false;

  while (!done) {
    if (m_async_listener_sp->GetEvent(event_sp, std::nullopt)) {
      uint32_t event_type = event_sp->GetType();

      LLDB_LOG(log,
               "ProcessTrace::%s(pid = %" PRIu64
               ") Got an event of type: %d...",
               __FUNCTION__, GetID(), event_type);

      switch (event_type) {
      case eBroadcastBitAsyncContinue: {
        const ProcessTraceEventData *event_data =
            ProcessTraceEventData::GetEventData(event_sp.get());

        if (!event_data)
          break;

        RunDirection direction = event_data->GetDirection();
        LLDB_LOG(log,
                 "ProcessTrace::%s(pid = %" PRIu64
                 ") Got async continue event, direction = %s.",
                 __FUNCTION__, GetID(),
                 direction == RunDirection::eRunForward ? "forward"
                                                        : "reverse");

        SetPrivateState(eStateRunning);

        Status status = AsyncHandleContinue(direction);
        if (status.Fail()) {
          LLDB_LOG(log,
                   "ProcessTrace::%s(pid = %" PRIu64
                   ") failed to handle async continue event: %s",
                   __FUNCTION__, GetID(), status.AsCString());
        }
        break;
      }
      }
    }
  }

  return 0;
}

Status ProcessTrace::AsyncHandleContinue(RunDirection direction) {
  // Update the direction of the cursors.
  for (const auto &thread_sp : m_thread_list.Threads()) {
    ThreadTraceSP thread_trace_sp =
        std::static_pointer_cast<ThreadTrace>(thread_sp);
    Expected<TraceCursorSP> cursor_or_error = thread_trace_sp->GetTraceCursor();
    if (!cursor_or_error) {
      return Status::FromError(cursor_or_error.takeError());
    }
    TraceCursorSP cursor_sp = *cursor_or_error;
    cursor_sp->SetForwards(direction == lldb::eRunForward);
  }

  bool should_stop = false;

  // Advance all threads together until we hit a reason to stop.
  // Global timestamps are used to keep multiple threads in sync.
  std::optional<uint64_t> global_tsc;
  while (!should_stop) {
    // Track the "slowest" thread so we don't advance any thread too far ahead of the others.
    std::optional<uint64_t> least_advanced_tsc;

    for (const auto &thread_sp : m_thread_list.Threads()) {
      std::optional<uint64_t> thread_tsc{global_tsc};
      Status status = ContinueSubsequence(thread_sp, global_tsc, should_stop);
      if (status.Fail()) {
        return status;
      }

      if (!least_advanced_tsc) {
        least_advanced_tsc = thread_tsc;
        continue;
      }

      if (thread_tsc &&
          (direction == lldb::eRunForward ? thread_tsc < least_advanced_tsc
                                          : thread_tsc > least_advanced_tsc)) {
        least_advanced_tsc = thread_tsc;
      }
    }

    // Advance the global timestamp with the slowest thread.
    global_tsc = least_advanced_tsc;
  }

  SetPrivateState(lldb::eStateStopped);

  return Status();
}

/// Continues until we find a reason to stop, or the end of the subsequence.
///
/// A sequence is a series of instructions ran by a thread.
/// A subsequence is a portion of a sequence between two timestamps.
/// Within subsequences, there is no defined order of trace items from different
/// threads, so the best we can do is keep threads in sync at the granularity of
/// a subsequence.
///
/// \param[in] tsc
///    The current global timestamp. Threads ahead of this timestamp should not continue.
/// \param[out] tsc
///    The current timestamp of this thread.
Status ProcessTrace::ContinueSubsequence(ThreadSP thread_sp,
                                         std::optional<uint64_t> &tsc,
                                         bool &should_stop) {
  ThreadTraceSP thread_trace_sp =
      std::static_pointer_cast<ThreadTrace>(thread_sp);
  Expected<TraceCursorSP> cursor_or_error = thread_trace_sp->GetTraceCursor();
  if (!cursor_or_error) {
    return Status::FromError(cursor_or_error.takeError());
  }
  TraceCursorSP cursor_sp = *cursor_or_error;

  // Check we're not already "ahead" of the global timestamp,
  // e.g. if a previous timestamp was omitted in the trace for whatever reason.
  // In this case we should let the other threads catch up before continuing.
  std::optional<uint64_t> thread_tsc = cursor_sp->GetHWClock();
  if (tsc && thread_tsc && (cursor_sp->IsForwards() ? thread_tsc > *tsc
                                      : thread_tsc < *tsc)) {
    tsc = thread_tsc;
    return Status();
  }

  // Do nothing if the thread is suspended.
  if (thread_sp->GetTemporaryResumeState() == eStateSuspended) {
    return Status();
  }

  // Continue until we hit a timestamp, end of trace, or place to stop at.
  for (cursor_sp->Next(); cursor_sp->HasValue(); cursor_sp->Next()) {
    if (cursor_sp->GetItemKind() == eTraceItemKindEvent &&
        cursor_sp->GetEventType() == eTraceEventHWClockTick) {
      tsc = cursor_sp->GetHWClock();
      return Status();
    }

    if (cursor_sp->GetItemKind() == eTraceItemKindInstruction) {
      // Update the thread's PC to the current instruction.
      // TODO: prevent unwinding at every step?
      thread_sp->GetRegisterContext()->SetPC(cursor_sp->GetLoadAddress());

      // Stop the process if we hit a breakpoint.
      if (GetBreakpointSiteList().FindByAddress(
              cursor_sp->GetLoadAddress())) {
        should_stop = true;
        return Status();
      }

      // Stop the process after one instruction if we are stepping.
      // TODO: is this really correct?
      if (thread_sp->GetTemporaryResumeState() == eStateStepping) {
        should_stop = true;
        return Status();
      }
    }
  }

  should_stop = true;
  return Status();
}

bool ProcessTrace::CalculateThreadStopInfo(ThreadTrace &thread) {
  Expected<TraceCursorSP> cursor_or_error = thread.GetTraceCursor();
  if (!cursor_or_error) {
    return false;
  }
  TraceCursorSP cursor_sp = *cursor_or_error;

  if (!cursor_sp->HasValue()) {
    thread.SetStopInfo(StopInfo::CreateStopReasonHistoryBoundary(thread, "End of Trace"));

    // In case we were executing a thread plan when we hit the end of the trace,
    // discard it so it doesn't affect future commands.
    // TODO: write a test for hitting the end when stepping over, then reverse
    // resuming
    thread.DiscardThreadPlans(true);

    return true;
  }

  if (cursor_sp->GetItemKind() == eTraceItemKindInstruction) {
    BreakpointSiteSP bp_site_sp =
        GetBreakpointSiteList().FindByAddress(cursor_sp->GetLoadAddress());
    if (bp_site_sp) {
      thread.SetStopInfo(StopInfo::CreateStopReasonWithBreakpointSiteID(
          thread, bp_site_sp->GetID()));
      return true;
    }

    if (thread.GetTemporaryResumeState() == eStateStepping) {
      thread.SetStopInfo(StopInfo::CreateStopReasonToTrace(thread));
      return true;
    }
  }

  return false;
}
