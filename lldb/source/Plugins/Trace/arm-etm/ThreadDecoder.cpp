//===-- ThreadDecoder.cpp --======-----------------------------------------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ThreadDecoder.h"
#include "../common/ThreadPostMortemTrace.h"
#include "OpenCSDDecoder.h"
#include "TraceArmETM.h"
#include "llvm/Support/MemoryBuffer.h"
#include <optional>
#include <utility>

using namespace lldb;
using namespace lldb_private;
using namespace trace_arm_etm;
using namespace llvm;

ThreadDecoder::ThreadDecoder(const ThreadSP &thread_sp, TraceArmETM &trace)
    : m_thread_sp(thread_sp), m_trace(trace) {}

Expected<DecodedThreadSP> ThreadDecoder::Decode() {
  if (!m_decoded_thread.has_value()) {
    if (Expected<DecodedThreadSP> decoded_thread = DoDecode()) {
      m_decoded_thread = *decoded_thread;
    } else {
      return decoded_thread.takeError();
    }
  }
  return *m_decoded_thread;
}

llvm::Expected<DecodedThreadSP> ThreadDecoder::DoDecode() {
  DecodedThreadSP decoded_thread_sp =
      std::make_shared<DecodedThread>(m_thread_sp);

  Error err = m_trace.OnThreadBufferRead(
      m_thread_sp->GetID(), [&](llvm::ArrayRef<uint8_t> data) {
        return DecodeSingleTraceForThread(*decoded_thread_sp, m_trace, data);
      });

  if (err)
    return std::move(err);
  return decoded_thread_sp;
}
