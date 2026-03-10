//===-- DecodedThread.cpp -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "DecodedThread.h"

using namespace lldb;
using namespace lldb_private;
using namespace trace_arm_etm;
using namespace llvm;

bool DecodedThread::TSCRange::InRange(uint64_t item_index) const {
  return item_index >= first_item_index &&
         item_index < first_item_index + items_count;
}

uint64_t DecodedThread::GetItemsCount() const { return m_item_data.size(); }

lldb::addr_t
DecodedThread::GetInstructionLoadAddress(uint64_t item_index) const {
  return std::get<lldb::addr_t>(m_item_data[item_index]);
}

ThreadSP DecodedThread::GetThread() { return m_thread_sp; }

template <typename Data>
DecodedThread::TraceItemStorage &
DecodedThread::CreateNewTraceItem(lldb::TraceItemKind kind, Data &&data) {
  m_item_data.emplace_back(data);

  if (m_last_tsc)
    (*m_last_tsc)->second.items_count++;

  return m_item_data.back();
}

void DecodedThread::NotifyTsc(TSC tsc) {
  if (m_last_tsc && (*m_last_tsc)->second.tsc == tsc)
    return;
  if (m_last_tsc)
    assert(tsc >= (*m_last_tsc)->second.tsc &&
           "We can't have decreasing times");

  m_last_tsc =
      m_tscs.emplace(GetItemsCount(), TSCRange{tsc, 0, GetItemsCount()}).first;

  AppendEvent(lldb::eTraceEventHWClockTick);
}

std::optional<DecodedThread::TSCRange>
DecodedThread::GetTSCRangeByIndex(uint64_t item_index) const {
  auto next_it = m_tscs.upper_bound(item_index);
  if (next_it == m_tscs.begin())
    return std::nullopt;
  return prev(next_it)->second;
}

void DecodedThread::AppendEvent(lldb::TraceEvent event) {
  CreateNewTraceItem(lldb::eTraceItemKindEvent, event);
}

void DecodedThread::AppendInstructionLoadAddress(lldb::addr_t load_address) {
  CreateNewTraceItem(lldb::eTraceItemKindInstruction, load_address);
  m_insn_count++;
}

void DecodedThread::AppendError(const ocsdError &error) {
  CreateNewTraceItem(lldb::eTraceItemKindError, error.getMessage());
}

void DecodedThread::AppendError(const std::string &error) {
  CreateNewTraceItem(lldb::eTraceItemKindError, error);
}

lldb::TraceEvent DecodedThread::GetEventByIndex(uint64_t item_index) const {
  return std::get<lldb::TraceEvent>(m_item_data[item_index]);
}

lldb::TraceItemKind
DecodedThread::GetItemKindByIndex(uint64_t item_index) const {
  return std::visit(
      llvm::makeVisitor(
          [](const std::string &) { return lldb::eTraceItemKindError; },
          [](lldb::TraceEvent) { return lldb::eTraceItemKindEvent; },
          [](lldb::addr_t) { return lldb::eTraceItemKindInstruction; }),
      m_item_data[item_index]);
}

llvm::StringRef DecodedThread::GetErrorByIndex(uint64_t item_index) const {
  if (item_index >= m_item_data.size())
    return llvm::StringRef();
  return std::get<std::string>(m_item_data[item_index]);
}

DecodedThread::DecodedThread(ThreadSP thread_sp) : m_thread_sp(thread_sp) {}