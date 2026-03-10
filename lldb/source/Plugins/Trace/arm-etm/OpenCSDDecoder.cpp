//===-- OpenCSDDecoder.cpp --======----------------------------------------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "OpenCSDDecoder.h"
#include "DecodedThread.h"
#include "TraceArmETM.h"
#include "lldb/Target/Process.h"
#include "llvm/Support/Endian.h"
#include <i_dec/trc_idec_arminst.h>
#include <opencsd.h>

using namespace lldb;
using namespace lldb_private;
using namespace trace_arm_etm;
using namespace llvm;

using DecodeTreeUP =
    std::unique_ptr<DecodeTree, decltype(&DecodeTree::DestroyDecodeTree)>;

using OcsdErrorUP = std::unique_ptr<ocsdError>;

class OpenCSDDecoder : public ITrcGenElemIn, ITargetMemAccess, ITraceErrorLog {
public:
  OpenCSDDecoder(const TraceArmETM &trace_arm_etm, ArrayRef<uint8_t> buffer,
                 ProcessSP process, DecodedThread &decoded_thread)
      : m_buffer(buffer), m_process(process), m_decoded_thread(decoded_thread),
        m_decode_tree_up(DecodeTree::CreateDecodeTree(
                             OCSD_TRC_SRC_SINGLE, OCSD_DFRMTR_FRAME_MEM_ALIGN),
                         DecodeTree::DestroyDecodeTree) {
    m_decode_tree_up->setMemAccessI(this);
    m_decode_tree_up->setGenTraceElemOutI(this);
    m_decode_tree_up->setAlternateErrorLogger(this);

    // Create a decoder for each trace unit in the trace
    for (const CSConfig &cfg : trace_arm_etm.GetTraceUnitConfigs()) {
      m_decode_tree_up->createDecoder(OCSD_BUILTIN_DCD_ETMV4I,
                                      OCSD_CREATE_FLG_FULL_DECODER, &cfg);
    }
  }

  Error Decode() {
    ocsd_trc_index_t index = 0;
    while (index < m_buffer.size()) {
      uint32_t num_bytes_processed = 0;

      ocsd_datapath_resp_t response = m_decode_tree_up->TraceDataIn(
          OCSD_OP_DATA, index, m_buffer.size() - index,
          m_buffer.slice(index).data(), &num_bytes_processed);

      while (OCSD_DATA_RESP_IS_WAIT(response))
        response = m_decode_tree_up->TraceDataIn(OCSD_OP_FLUSH, 0, 0, nullptr,
                                                 nullptr);

      if (OCSD_DATA_RESP_IS_FATAL(response))
        return createStringError(inconvertibleErrorCode(),
                                 ocsdDataRespStr(response).getStr());

      // Our ITrcGenElemIn implementation never returns a WAIT response,
      // so the response must be CONT here.
      assert(OCSD_DATA_RESP_IS_CONT(response));

      index += num_bytes_processed;
    }

    // Mark end of trace
    m_decode_tree_up->TraceDataIn(OCSD_OP_EOT, 0, 0, nullptr, nullptr);

    return Error::success();
  }

  /// ITrcGenElemIn protocol
  /// \{
  ocsd_datapath_resp_t TraceElemIn(const ocsd_trc_index_t index_sop,
                                   const uint8_t trc_chan_id,
                                   const OcsdTraceElement &elem) override {
    switch (elem.getType()) {
    case OCSD_GEN_TRC_ELEM_UNKNOWN:
      m_decoded_thread.AppendError("unknown trace element");
      return OCSD_RESP_CONT;
    case OCSD_GEN_TRC_ELEM_INSTR_RANGE: {
      addr_t addr = elem.st_addr;
      do {
        m_decoded_thread.AppendInstructionLoadAddress(addr);
      } while (addr < elem.en_addr && NextInstruction(elem.isa, addr));

      return OCSD_RESP_CONT;
    }
    case OCSD_GEN_TRC_ELEM_I_RANGE_NOPATH:
      // Not enough information to determine complete program flow
      // We can only be sure that the start address was executed
      m_decoded_thread.AppendInstructionLoadAddress(elem.st_addr);
      // TODO: go through range looking for branch.
      // At minimum, we can determine the program flow up to any branch.
      // Whether a branch was taken or not could potentially be inferred based on future trace elements
      return OCSD_RESP_CONT;
    case OCSD_GEN_TRC_ELEM_TIMESTAMP:
      m_decoded_thread.NotifyTsc(elem.timestamp);
      return OCSD_RESP_CONT;
    case OCSD_GEN_TRC_ELEM_ADDR_NACC:
      m_decoded_thread.AppendError("tracing in inaccessible memory area");
      return OCSD_RESP_CONT;
    case OCSD_GEN_TRC_ELEM_ADDR_UNKNOWN:
      m_decoded_thread.AppendError("trace address unknown");
      return OCSD_RESP_CONT;
    default:
      return OCSD_RESP_CONT;
    }
  }
  /// \}

  /// ITargetMemAccess protocol
  /// \{
  ocsd_err_t ReadTargetMemory(const ocsd_vaddr_t address,
                              const uint8_t cs_trace_id,
                              const ocsd_mem_space_acc_t mem_space,
                              uint32_t *num_bytes, uint8_t *p_buffer) override {
    Status error;
    int bytes_read =
        m_process->ReadMemory(address, p_buffer, *num_bytes, error);
    if (error.Fail()) {
      return OCSD_ERR_MEM_ACC_RANGE_INVALID;
    }
    *num_bytes = bytes_read;
    return OCSD_OK;
  }

  void InvalidateMemAccCache(const uint8_t cs_trace_id) override {}
  /// \}

  /// ITraceErrorLog protocol
  /// \{

// These `const` qualifiers have no effect, but are required to match the
// declaration from OpenCSD, so suppress the warning about them being ignored.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-qualifiers"
  const ocsd_hndl_err_log_t
  RegisterErrorSource(const std::string &component_name) override {
    return 0;
  }

  const ocsd_err_severity_t GetErrorLogVerbosity() const override {
    return OCSD_ERR_SEV_WARN;
  }
#pragma clang diagnostic pop

  void LogError(const ocsd_hndl_err_log_t handle,
                const ocsdError *Error) override {
    m_decoded_thread.AppendError(*Error);

    // Copy error and save it for later retrieval
    uint8_t chan_id = Error->getErrorChanID();
    m_last_errors[chan_id] = std::make_unique<ocsdError>(*Error);
    m_last_error_chan_id = chan_id;
  }

  void LogMessage(const ocsd_hndl_err_log_t handle,
                  const ocsd_err_severity_t filter_level,
                  const std::string &msg) override {
    m_decoded_thread.AppendError(msg);
  }

  ocsdError *GetLastError() override {
    if (!m_last_error_chan_id.has_value()) {
      return nullptr;
    }

    return m_last_errors[*m_last_error_chan_id].get();
  }

  ocsdError *GetLastIDError(const uint8_t chan_id) override {
    if (auto it = m_last_errors.find(chan_id); it != m_last_errors.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  ocsdMsgLogger *getOutputLogger() override { return m_msg_logger; }
  void setOutputLogger(ocsdMsgLogger *pLogger) override {
    m_msg_logger = pLogger;
  }
  /// \}
private:
  /// Advance addr to the next instruction and return \b false if we should
  /// stop.
  bool NextInstruction(ocsd_isa isa, addr_t &addr) {
    if (isa == ocsd_isa_thumb2) {
      uint8_t instr_bytes[2];

      Status error;
      size_t bytes_read =
          m_process->ReadMemory(addr, instr_bytes, sizeof(instr_bytes), error);
      if (error.Fail() || bytes_read != sizeof(instr_bytes)) {
        m_decoded_thread.AppendError("failed to read instruction from memory");
        return false;
      }

      uint16_t instr_halfword = llvm::support::endian::read16le(instr_bytes);

      // Thumb instructions can be either 2 or 4 bytes
      addr += is_wide_thumb(instr_halfword) ? 4 : 2;
      return true;
    }

    // Assume 4 byte instruction size (A32/A64)
    addr += 4;
    return true;
  }

  ArrayRef<uint8_t> m_buffer;
  ProcessSP m_process;
  DecodedThread &m_decoded_thread;

  DecodeTreeUP m_decode_tree_up;

  /// Maps trace source channel IDs -> last error associated with each channel,
  /// if any.
  DenseMap<uint8_t, OcsdErrorUP> m_last_errors;
  /// The last channel to have an error, if any.
  std::optional<uint8_t> m_last_error_chan_id;

  ocsdMsgLogger *m_msg_logger;
};

Error lldb_private::trace_arm_etm::DecodeSingleTraceForThread(
    DecodedThread &decoded_thread, TraceArmETM &trace_arm_etm,
    ArrayRef<uint8_t> buffer) {
  OpenCSDDecoder decoder{trace_arm_etm, buffer,
                         decoded_thread.GetThread()->GetProcess(),
                         decoded_thread};

  return decoder.Decode();
}
