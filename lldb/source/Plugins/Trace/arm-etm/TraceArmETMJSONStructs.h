//===-- TraceArmETMJSONStructs.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_TRACE_ARM_ETM_TRACEARMETMJSONSTRUCTS_H
#define LLDB_SOURCE_PLUGINS_TRACE_ARM_ETM_TRACEARMETMJSONSTRUCTS_H

#include "../common/TraceJSONStructs.h"
#include "lldb/lldb-types.h"
#include "llvm/Support/JSON.h"
#include <opencsd.h>
#include <optional>
#include <vector>

namespace lldb_private {
namespace trace_arm_etm {

struct JSONThread {
  uint64_t tid;
  std::optional<std::string> etm_trace;
};

struct JSONProcess {
  uint64_t pid;
  std::optional<std::string> triple;
  std::vector<JSONThread> threads;
  std::vector<JSONModule> modules;
};

struct JSONTraceUnit {
  JSONUINT64 reg_idr0;
  JSONUINT64 reg_idr1;
  JSONUINT64 reg_idr2;
  JSONUINT64 reg_idr8;
  JSONUINT64 reg_idr9;
  JSONUINT64 reg_idr10;
  JSONUINT64 reg_idr11;
  JSONUINT64 reg_idr12;
  JSONUINT64 reg_idr13;
  JSONUINT64 reg_configr;
  JSONUINT64 reg_traceidr;
  ocsd_arch_version_t arch_ver;
  ocsd_core_profile_t core_prof;

  std::unique_ptr<CSConfig> MakeCSConfig();
};

struct JSONTraceBundleDescription {
  std::string type;
  std::vector<JSONTraceUnit> trace_units;
  std::optional<std::vector<JSONProcess>> processes;
};

llvm::json::Value toJSON(const JSONThread &thread);

llvm::json::Value toJSON(const JSONProcess &process);

llvm::json::Value toJSON(const JSONTraceUnit &trace_unit);

llvm::json::Value toJSON(const JSONTraceBundleDescription &bundle_description);

bool fromJSON(const llvm::json::Value &value, JSONThread &thread,
              llvm::json::Path path);

bool fromJSON(const llvm::json::Value &value, JSONProcess &process,
              llvm::json::Path path);

bool fromJSON(const llvm::json::Value &value, JSONTraceUnit &trace_unit,
              llvm::json::Path path);

bool fromJSON(const llvm::json::Value &value,
              JSONTraceBundleDescription &bundle_description,
              llvm::json::Path path);
} // namespace trace_arm_etm
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_TRACE_ARM_ETM_TRACEARMETMJSONSTRUCTS_H
