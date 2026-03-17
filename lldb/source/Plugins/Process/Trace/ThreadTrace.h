//===-- ThreadTrace.h -------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_PROCESS_TRACE_THREADTRACE_H
#define LLDB_SOURCE_PLUGINS_PROCESS_TRACE_THREADTRACE_H

#include "lldb/lldb-forward.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/TraceCursor.h"
#include "llvm/Support/Error.h"
#include <optional>

namespace lldb_private {

/// \class ThreadTrace ThreadTrace.h
///
/// Thread implementation used for representing threads gotten from trace
/// session files, which are similar to threads from core files.
///
class ThreadTrace : public Thread {
public:
  /// \param[in] process
  ///     The process who owns this thread.
  ///
  /// \param[in] tid
  ///     The tid of this thread.
  ///
  /// \param[in] trace_file
  ///     The file that contains the list of instructions that were traced when
  ///     this thread was being executed.
  ThreadTrace(Process &process, lldb::tid_t tid,
                        const std::optional<FileSpec> &trace_file)
      : Thread(process, tid), m_trace_file(trace_file) {}

  void RefreshStateAfterStop() override;

  lldb::RegisterContextSP GetRegisterContext() override;

  lldb::RegisterContextSP
  CreateRegisterContextForFrame(StackFrame *frame) override;

  bool CalculateStopInfo() override;

  /// \return
  ///   The trace file of this thread.
  const std::optional<FileSpec> &GetTraceFile() const;

  llvm::Expected<lldb::TraceCursorSP> GetTraceCursor();

private:
  std::optional<FileSpec> m_trace_file;

  lldb::TraceCursorSP m_cursor_sp;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_PROCESS_TRACE_THREADTRACE_H
