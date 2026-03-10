//===-- TraceCursorArmETM.h ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_TRACE_INTEL_PT_TRACECURSORINTELPT_H
#define LLDB_SOURCE_PLUGINS_TRACE_INTEL_PT_TRACECURSORINTELPT_H

#include "ThreadDecoder.h"
#include <optional>

namespace lldb_private {
namespace trace_arm_etm {

class TraceCursorArmETM : public TraceCursor {
public:
  TraceCursorArmETM(lldb::ThreadSP thread_sp,
                    DecodedThreadSP decoded_thread_sp);

  bool Seek(int64_t offset, lldb::TraceCursorSeekType origin) override;

  void Next() override;

  bool HasValue() const override;

  llvm::StringRef GetError() const override;

  lldb::addr_t GetLoadAddress() const override;

  lldb::TraceEvent GetEventType() const override;

  lldb::cpu_id_t GetCPU() const override;

  std::optional<uint64_t> GetHWClock() const override;

  lldb::TraceItemKind GetItemKind() const override;

  bool GoToId(lldb::user_id_t id) override;

  lldb::user_id_t GetId() const override;

  bool HasId(lldb::user_id_t id) const override;

  std::optional<double> GetWallClockTime() const override;

  std::optional<std::string> GetSyncPointMetadata() const override;

private:
  /// Clear the current TSC range if after moving they are not valid anymore.
  void ClearTimingRangesIfInvalid();

  /// Get or calculate the TSC range that includes the current trace item.
  const std::optional<DecodedThread::TSCRange> &GetTSCRange() const;

  /// Storage of the actual instructions
  DecodedThreadSP m_decoded_thread_sp;

  /// Internal trace element index currently pointing at.
  int64_t m_pos;

  /// Timing information and cached values.
  /// \{

  /// Range of trace items with the same TSC that includes the current trace
  /// item, \a std::nullopt if not calculated or not available.
  std::optional<DecodedThread::TSCRange> mutable m_tsc_range;
  bool mutable m_tsc_range_calculated = false;
  /// \}
};

} // namespace trace_arm_etm
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_TRACE_INTEL_PT_TRACECURSORINTELPT_H
