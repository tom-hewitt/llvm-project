//===-- forward-declarations.h ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_PROCESS_TRACE_FORWARD_DECLARATIONS_H
#define LLDB_SOURCE_PLUGINS_PROCESS_TRACE_FORWARD_DECLARATIONS_H

#include <memory>

namespace lldb_private {

class ThreadTrace;

using ThreadTraceSP = std::shared_ptr<ThreadTrace>;

} // namespace lldb_private
#endif // LLDB_SOURCE_PLUGINS_PROCESS_TRACE_FORWARD_DECLARATIONS_H
