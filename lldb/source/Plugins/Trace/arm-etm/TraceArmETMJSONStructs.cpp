//===-- TraceArmETMJSONStructs.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TraceArmETMJSONStructs.h"
#include "llvm/Support/JSON.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::trace_arm_etm;
using namespace llvm;
using namespace llvm::json;

namespace lldb_private {
namespace trace_arm_etm {

std::unique_ptr<CSConfig> JSONTraceUnit::MakeCSConfig() {
  ocsd_etmv4_cfg etm_cfg = {
      .reg_idr0 = static_cast<uint32_t>(reg_idr0.value),
      .reg_idr1 = static_cast<uint32_t>(reg_idr1.value),
      .reg_idr2 = static_cast<uint32_t>(reg_idr2.value),
      .reg_idr8 = static_cast<uint32_t>(reg_idr8.value),
      .reg_idr9 = static_cast<uint32_t>(reg_idr9.value),
      .reg_idr10 = static_cast<uint32_t>(reg_idr10.value),
      .reg_idr11 = static_cast<uint32_t>(reg_idr11.value),
      .reg_idr12 = static_cast<uint32_t>(reg_idr12.value),
      .reg_idr13 = static_cast<uint32_t>(reg_idr13.value),
      .reg_configr = static_cast<uint32_t>(reg_configr.value),
      .reg_traceidr = static_cast<uint32_t>(reg_traceidr.value),
      .arch_ver = arch_ver,
      .core_prof = core_prof,
  };

  return std::make_unique<EtmV4Config>(&etm_cfg);
}

json::Value toJSON(const JSONThread &thread) {
  json::Object obj{{"tid", thread.tid}};
  if (thread.etm_trace)
    obj["etmTrace"] = *thread.etm_trace;
  return obj;
}

bool fromJSON(const json::Value &value, JSONThread &thread, Path path) {
  ObjectMapper o(value, path);
  return o && o.map("tid", thread.tid) && o.map("etmTrace", thread.etm_trace);
}

json::Value toJSON(const JSONProcess &process) {
  return Object{
      {"pid", process.pid},
      {"triple", process.triple},
      {"threads", process.threads},
      {"modules", process.modules},
  };
}

bool fromJSON(const json::Value &value, JSONProcess &process, Path path) {
  ObjectMapper o(value, path);
  return o && o.map("pid", process.pid) && o.map("triple", process.triple) &&
         o.map("threads", process.threads) && o.map("modules", process.modules);
}

StringRef toString(const ocsd_arch_version_t &arch_version) {
  switch (arch_version) {
  case ARCH_UNKNOWN:
    return "unknown";
  case ARCH_CUSTOM:
    return "custom";
  case ARCH_V7:
    return "v7";
  case ARCH_V8:
    return "v8";
  case ARCH_V8r3:
    return "v8r3";
  case ARCH_AA64:
    return "aa64";
  }
}

ocsd_arch_version_t archVersionFromString(const StringRef &value) {
  return StringSwitch<ocsd_arch_version_t>(value)
      .Case("custom", ARCH_CUSTOM)
      .Case("v7", ARCH_V7)
      .Case("v8", ARCH_V8)
      .Case("v8r3", ARCH_V8r3)
      .Case("aa64", ARCH_AA64)
      .Default(ARCH_UNKNOWN);
}

StringRef toString(const ocsd_core_profile_t &core_prof) {
  switch (core_prof) {
  case profile_Unknown:
    return "unknown";
  case profile_CortexM:
    return "Cortex-M";
  case profile_CortexR:
    return "Cortex-R";
  case profile_CortexA:
    return "Cortex-A";
  case profile_Custom:
    return "custom";
  }
}

ocsd_core_profile_t coreProfileFromString(const StringRef &value) {
  return StringSwitch<ocsd_core_profile_t>(value)
      .Case("Cortex-M", profile_CortexM)
      .Case("Cortex-R", profile_CortexR)
      .Case("Cortex-A", profile_CortexA)
      .Case("custom", profile_Custom)
      .Default(profile_Unknown);
}

bool fromJSON(const json::Value &value, JSONTraceUnit &trace_unit, Path path) {
  ObjectMapper o(value, path);
  std::string arch_version_str;
  std::string core_profile_str;
  if (!(o && o.map("regIdr0", trace_unit.reg_idr0) &&
        o.map("regIdr1", trace_unit.reg_idr1) &&
        o.map("regIdr2", trace_unit.reg_idr2) &&
        o.map("regIdr8", trace_unit.reg_idr8) &&
        o.map("regIdr9", trace_unit.reg_idr9) &&
        o.map("regIdr10", trace_unit.reg_idr10) &&
        o.map("regIdr11", trace_unit.reg_idr11) &&
        o.map("regIdr12", trace_unit.reg_idr12) &&
        o.map("regIdr13", trace_unit.reg_idr13) &&
        o.map("regConfigr", trace_unit.reg_configr) &&
        o.map("regTraceidr", trace_unit.reg_traceidr) &&
        o.map("archVersion", arch_version_str) &&
        o.map("coreProfile", core_profile_str)))
    return false;

  trace_unit.arch_ver = archVersionFromString(arch_version_str);
  trace_unit.core_prof = coreProfileFromString(core_profile_str);
  return true;
}

json::Value toJSON(const JSONTraceUnit &trace_unit) {
  return Object{
      {"regIdr0", toJSON(trace_unit.reg_idr0, true)},
      {"regIdr1", toJSON(trace_unit.reg_idr1, true)},
      {"regIdr2", toJSON(trace_unit.reg_idr2, true)},
      {"regIdr8", toJSON(trace_unit.reg_idr8, true)},
      {"regIdr9", toJSON(trace_unit.reg_idr9, true)},
      {"regIdr10", toJSON(trace_unit.reg_idr10, true)},
      {"regIdr11", toJSON(trace_unit.reg_idr11, true)},
      {"regIdr12", toJSON(trace_unit.reg_idr12, true)},
      {"regIdr13", toJSON(trace_unit.reg_idr13, true)},
      {"regConfigr", toJSON(trace_unit.reg_configr, true)},
      {"regTraceidr", toJSON(trace_unit.reg_traceidr, true)},
      {"archVersion", toString(trace_unit.arch_ver)},
      {"coreProfile", toString(trace_unit.core_prof)},
  };
}

json::Value toJSON(const JSONTraceBundleDescription &bundle_description) {
  return Object{
      {"type", bundle_description.type},
      {"traceUnits", bundle_description.trace_units},
      {"processes", bundle_description.processes},
  };
}

bool fromJSON(const json::Value &value,
              JSONTraceBundleDescription &bundle_description, Path path) {
  ObjectMapper o(value, path);

  return o && o.map("processes", bundle_description.processes) &&
         o.map("traceUnits", bundle_description.trace_units) &&
         o.map("type", bundle_description.type);
}

} // namespace trace_arm_etm
} // namespace lldb_private
