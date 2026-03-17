//===-- ThreadTrace.cpp ---------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ThreadTrace.h"

#include <memory>
#include <optional>

#include "ProcessTrace.h"
#include "RegisterContextTrace.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StopInfo.h"

using namespace lldb;
using namespace lldb_private;
using namespace llvm;

void ThreadTrace::RefreshStateAfterStop() {
  if (m_reg_context_sp)
    m_reg_context_sp->InvalidateAllRegisters();
}

RegisterContextSP ThreadTrace::GetRegisterContext() {
  if (!m_reg_context_sp)
    m_reg_context_sp = CreateRegisterContextForFrame(nullptr);

  return m_reg_context_sp;
}

RegisterContextSP
ThreadTrace::CreateRegisterContextForFrame(StackFrame *frame) {
  // Eventually this will calculate the register context based on the current
  // trace position.
  return std::make_shared<RegisterContextTrace>(
      *this, 0, GetProcess()->GetAddressByteSize(), LLDB_INVALID_ADDRESS);
}

bool ThreadTrace::CalculateStopInfo() {
  ProcessSP process_sp(GetProcess());
  if (process_sp)
    return static_cast<ProcessTrace *>(process_sp.get())
        ->CalculateThreadStopInfo(*this);
  return false;
}

const std::optional<FileSpec> &ThreadTrace::GetTraceFile() const {
  return m_trace_file;
}

Expected<TraceCursorSP> ThreadTrace::GetTraceCursor() {
  if (m_cursor_sp)
    return m_cursor_sp;

  // Create a new cursor for this thread at the end of the thread's trace
  Expected<TraceCursorSP> cursor_or_error =
      GetProcess()->GetTarget().GetTrace()->CreateNewCursor(*this);
  if (cursor_or_error)
    m_cursor_sp = *cursor_or_error;

  return cursor_or_error;
}
