add_lldb_library(lldbPluginTraceCommon
  TraceBundleLoader.cpp
  TraceJSONStructs.cpp

  LINK_LIBS
    lldbCore
    lldbTarget
    lldbPluginProcessTrace
  )
