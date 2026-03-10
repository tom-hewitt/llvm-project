//===-- DecodedThread.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_TRACE_ARM_ETM_DECODEDTHREAD_H
#define LLDB_SOURCE_PLUGINS_TRACE_ARM_ETM_DECODEDTHREAD_H

#include "lldb/Target/Trace.h"
#include "lldb/lldb-types.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Error.h"
#include <deque>
#include <opencsd.h>
#include <optional>
#include <utility>
#include <variant>

namespace lldb_private {
namespace trace_arm_etm {

/// \class DecodedThread
/// Class holding the instructions and function call hierarchy obtained from
/// decoding a trace, as well as a position cursor used when reverse debugging
/// the trace.
///
/// Each decoded thread contains a cursor to the current position the user is
/// stopped at. See \a Trace::GetCursorPosition for more information.
class DecodedThread : public std::enable_shared_from_this<DecodedThread> {
public:
  using TSC = uint64_t;

  /// A structure that represents a maximal range of trace items associated to
  /// the same TSC value.
  struct TSCRange {
    TSC tsc;
    /// Number of trace items in this range.
    uint64_t items_count;
    /// Index of the first trace item in this range.
    uint64_t first_item_index;

    /// \return
    ///   \b true if and only if the given \p item_index is covered by this
    ///   range.
    bool InRange(uint64_t item_index) const;
  };

  DecodedThread(lldb::ThreadSP thread_sp);

  /// Get the total number of instruction ranges, errors and events from the
  /// decoded trace.
  uint64_t GetItemsCount() const;

  lldb::ThreadSP GetThread();

  /// \return
  ///   The error associated with a given trace item.
  llvm::StringRef GetErrorByIndex(uint64_t item_index) const;

  /// \return
  ///   The trace item kind given an item index.
  lldb::TraceItemKind GetItemKindByIndex(uint64_t item_index) const;

  /// \return
  ///   The underlying event type for the given trace item index.
  lldb::TraceEvent GetEventByIndex(uint64_t item_index) const;

  /// Get a maximal range of trace items that include the given \p item_index
  /// that have the same TSC value.
  ///
  /// \param[in] item_index
  ///   The trace item index to compare with.
  ///
  /// \return
  ///   The requested TSC range, or \a std::nullopt if not available.
  std::optional<DecodedThread::TSCRange>
  GetTSCRangeByIndex(uint64_t item_index) const;

  /// \return
  ///     The load address range of the instructions at the given index.
  lldb::addr_t GetInstructionLoadAddress(uint64_t item_index) const;

  /// Notify this object that a new tsc has been seen.
  /// If this a new TSC, an event will be created.
  void NotifyTsc(TSC tsc);

  /// Append a decoding error.
  void AppendError(const ocsdError &error);

  /// Append a decoding error message.
  void AppendError(const std::string &msg);

  /// Append an event.
  void AppendEvent(lldb::TraceEvent);

  /// Append a range of consecutive instructions.
  void AppendInstructionLoadAddress(lldb::addr_t load_address);

private:
  lldb::ThreadSP m_thread_sp;

  using TraceItemStorage =
      std::variant<std::string, lldb::TraceEvent, lldb::addr_t>;

  /// Create a new trace item.
  ///
  /// \return
  ///   The index of the new item.
  template <typename Data>
  DecodedThread::TraceItemStorage &CreateNewTraceItem(lldb::TraceItemKind kind,
                                                      Data &&data);

  /// Most of the trace data is stored here.
  std::deque<TraceItemStorage> m_item_data;

  /// This map contains the TSCs of the decoded trace items. It maps
  /// `item index -> TSC`, where `item index` is the first index
  /// at which the mapped TSC first appears. We use this representation because
  /// TSCs are sporadic and we can think of them as ranges.
  std::map<uint64_t, TSCRange> m_tscs;
  /// This is the chronologically last TSC that has been added.
  std::optional<std::map<uint64_t, TSCRange>::iterator> m_last_tsc =
      std::nullopt;

  /// Total number of instructions in the trace.
  uint64_t m_insn_count = 0;
};

using DecodedThreadSP = std::shared_ptr<DecodedThread>;

} // namespace trace_arm_etm
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_TRACE_ARM_ETM_DECODEDTHREAD_H
