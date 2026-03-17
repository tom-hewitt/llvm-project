//===-- ProcessTrace.h ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_TARGET_PROCESS_TRACE_PROCESSTRACE_H
#define LLDB_TARGET_PROCESS_TRACE_PROCESSTRACE_H

#include "ThreadTrace.h"
#include "lldb/Host/HostThread.h"
#include "lldb/Target/PostMortemProcess.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Status.h"

namespace lldb_private {

/// Class that represents a defunct process loaded on memory via the "trace
/// load" command.
class ProcessTrace : public PostMortemProcess {
public:
  static void Initialize();

  static void Terminate();

  static llvm::StringRef GetPluginNameStatic() { return "trace"; }

  static llvm::StringRef GetPluginDescriptionStatic();

  ProcessTrace(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp,
               const FileSpec &core_file);

  ~ProcessTrace() override;

  bool CanDebug(lldb::TargetSP target_sp,
                bool plugin_specified_by_name) override;

  void DidAttach(ArchSpec &process_arch) override;

  DynamicLoader *GetDynamicLoader() override { return nullptr; }

  SystemRuntime *GetSystemRuntime() override { return nullptr; }

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

  Status DoDestroy() override;

  void RefreshStateAfterStop() override;

  bool SupportsReverseDirection() override { return true; }

  Status DoResume(lldb::RunDirection direction) override;

  bool WarnBeforeDetach() const override { return false; }

  Status EnableBreakpointSite(BreakpointSite *bp_site) override;

  Status DisableBreakpointSite(BreakpointSite *bp_site) override;

  size_t ReadMemory(lldb::addr_t addr, void *buf, size_t size,
                    Status &error) override;

  size_t DoReadMemory(lldb::addr_t addr, void *buf, size_t size,
                      Status &error) override;

  ArchSpec GetArchitecture();

  bool GetProcessInfo(ProcessInstanceInfo &info) override;

  class ProcessTraceEventData : public EventData {
  public:
    ProcessTraceEventData(lldb::RunDirection direction)
        : m_direction(direction) {}

    static llvm::StringRef GetFlavorString() { return "ProcessTraceEventData"; }

    llvm::StringRef GetFlavor() const override { return GetFlavorString(); }

    void Dump(Stream *s) const override {}

    lldb::RunDirection GetDirection() const { return m_direction; }

    static const ProcessTraceEventData *GetEventData(const Event *event_ptr);

  private:
    lldb::RunDirection m_direction;
  };

protected:
  void Clear();

  bool DoUpdateThreadList(ThreadList &old_thread_list,
                          ThreadList &new_thread_list) override;

private:
  friend class ThreadTrace;

  static lldb::ProcessSP CreateInstance(lldb::TargetSP target_sp,
                                        lldb::ListenerSP listener_sp,
                                        const FileSpec *crash_file_path,
                                        bool can_connect);

  enum {
    eBroadcastBitAsyncContinue = (1 << 0),
    eBroadcastBitAsyncThreadShouldExit = (1 << 1)
  };

  /// Starts a separate thread to replay the trace.
  bool StartAsyncThread();

  /// The entry point for the async thread that replays the trace.
  lldb::thread_result_t AsyncThread();

  /// Continues the process in the given direction from within the async thread.
  Status AsyncHandleContinue(lldb::RunDirection direction);

  Status ContinueSubsequence(lldb::ThreadSP thread_sp,
                            std::optional<uint64_t> &tsc,
                            bool &stopped);
  
  bool CalculateThreadStopInfo(ThreadTrace &thread_sp);

  lldb_private::Broadcaster m_async_broadcaster;
  lldb::ListenerSP m_async_listener_sp;
  lldb_private::HostThread m_async_thread;
};

} // namespace lldb_private

#endif // LLDB_TARGET_PROCESS_TRACE_PROCESSTRACE_H
