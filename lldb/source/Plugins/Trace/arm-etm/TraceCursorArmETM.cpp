//===-- TraceCursorArmETM.cpp --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TraceCursorArmETM.h"
#include "DecodedThread.h"
#include "TraceArmETM.h"
#include <cstdlib>
#include <optional>

using namespace lldb;
using namespace lldb_private;
using namespace trace_arm_etm;
using namespace llvm;

TraceCursorArmETM::TraceCursorArmETM(ThreadSP thread_sp,
                                     DecodedThreadSP decoded_thread_sp)
    : TraceCursor(thread_sp), m_decoded_thread_sp(decoded_thread_sp) {
  Seek(0, lldb::eTraceCursorSeekTypeEnd);
}

void TraceCursorArmETM::Next() {
  m_pos += IsForwards() ? 1 : -1;
  ClearTimingRangesIfInvalid();
}

void TraceCursorArmETM::ClearTimingRangesIfInvalid() {
  if (m_tsc_range_calculated) {
    if (!m_tsc_range || m_pos < 0 || !m_tsc_range->InRange(m_pos)) {
      m_tsc_range = std::nullopt;
      m_tsc_range_calculated = false;
    }
  }
}

const std::optional<DecodedThread::TSCRange> &
TraceCursorArmETM::GetTSCRange() const {
  if (!m_tsc_range_calculated) {
    m_tsc_range_calculated = true;
    m_tsc_range = m_decoded_thread_sp->GetTSCRangeByIndex(m_pos);
  }
  return m_tsc_range;
}

bool TraceCursorArmETM::Seek(int64_t offset, lldb::TraceCursorSeekType origin) {
  switch (origin) {
  case lldb::eTraceCursorSeekTypeBeginning:
    m_pos = offset;
    break;
  case lldb::eTraceCursorSeekTypeEnd:
    m_pos = m_decoded_thread_sp->GetItemsCount() - 1 + offset;
    break;
  case lldb::eTraceCursorSeekTypeCurrent:
    m_pos += offset;
    break;
  }

  return HasValue();
}

bool TraceCursorArmETM::HasValue() const {
  return m_pos >= 0 &&
         static_cast<uint64_t>(m_pos) < m_decoded_thread_sp->GetItemsCount();
}

lldb::TraceItemKind TraceCursorArmETM::GetItemKind() const {
  return m_decoded_thread_sp->GetItemKindByIndex(m_pos);
}

llvm::StringRef TraceCursorArmETM::GetError() const {
  return m_decoded_thread_sp->GetErrorByIndex(m_pos);
}

lldb::addr_t TraceCursorArmETM::GetLoadAddress() const {
  return m_decoded_thread_sp->GetInstructionLoadAddress(m_pos);
}

std::optional<uint64_t> TraceCursorArmETM::GetHWClock() const {
  if (const std::optional<DecodedThread::TSCRange> &range = GetTSCRange())
    return range->tsc;
  return std::nullopt;
}

std::optional<double> TraceCursorArmETM::GetWallClockTime() const {
  llvm_unreachable("Unimplemented");
}

lldb::cpu_id_t TraceCursorArmETM::GetCPU() const {
  llvm_unreachable("Unimplemented");
}

lldb::TraceEvent TraceCursorArmETM::GetEventType() const {
  return m_decoded_thread_sp->GetEventByIndex(m_pos);
}

bool TraceCursorArmETM::GoToId(user_id_t id) {
  if (!HasId(id))
    return false;
  m_pos = id;
  return true;
}

bool TraceCursorArmETM::HasId(lldb::user_id_t id) const {
  return id < m_decoded_thread_sp->GetItemsCount();
}

user_id_t TraceCursorArmETM::GetId() const { return m_pos; }

std::optional<std::string> TraceCursorArmETM::GetSyncPointMetadata() const {
  llvm_unreachable("Unimplemented");
}
