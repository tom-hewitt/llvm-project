//===-- OpenCSDDecoder.h --======--------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_TRACE_ARM_ETM_OPENCSDDECODER_H
#define LLDB_SOURCE_PLUGINS_TRACE_ARM_ETM_OPENCSDDECODER_H

#include "DecodedThread.h"
#include "TraceArmETM.h"

namespace lldb_private {
namespace trace_arm_etm {

/// Decode a raw Arm ETM trace for a single thread given in \p buffer and
/// append the decoded instructions and errors in \p decoded_thread. It uses the
/// low level opencsd library underneath.
///
/// \return
///   An \a llvm::Error if the decoder couldn't be properly set up.
llvm::Error DecodeSingleTraceForThread(DecodedThread &decoded_thread,
                                       TraceArmETM &trace_arm_etm,
                                       llvm::ArrayRef<uint8_t> buffer);

} // namespace trace_arm_etm
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_TRACE_ARM_ETM_OPENCSDDECODER_H