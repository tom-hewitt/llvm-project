if (NOT OPENCSD_INCLUDE_PATH)
  message (FATAL_ERROR "OpenCSD include path not provided")
endif()

if (NOT EXISTS "${OPENCSD_INCLUDE_PATH}")
  message (FATAL_ERROR "invalid OpenCSD include path provided")
endif()

message(STATUS "Using OpenCSD include path: ${OPENCSD_INCLUDE_PATH}")

find_library(OPENCSD_LIBRARY opencsd PATHS ${OPENCSD_LIBRARY_PATH} REQUIRED)

add_lldb_library(lldbPluginTraceArmETM PLUGIN
  DecodedThread.cpp
  OpenCSDDecoder.cpp
  ThreadDecoder.cpp
  TraceCursorArmETM.cpp
  TraceArmETM.cpp
  TraceArmETMJSONStructs.cpp
  TraceArmETMBundleLoader.cpp

  LINK_COMPONENTS
    Support
  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
    lldbPluginTraceCommon
    ${OPENCSD_LIBRARY}
  )

# Mark OpenCSD headers as SYSTEM includes to suppress warnings from third-party code.
target_include_directories(lldbPluginTraceArmETM SYSTEM PRIVATE ${OPENCSD_INCLUDE_PATH})

# OpenCSD uses C++ exceptions internally, but converts them to error codes at the API boundary.
# LLDB is built with exceptions disabled, so can't contain the (unused) OpenCSD headers containing internal exceptions.
# Exclude these unused headers by pretending they've already been included.
target_compile_definitions(lldbPluginTraceArmETM PRIVATE ARM_TRC_PKT_PROC_PTM_H_INCLUDED)