add_lldb_library(lldbPluginProcessTrace PLUGIN
  ProcessTrace.cpp
  ThreadTrace.cpp
  RegisterContextTrace.cpp

  LINK_COMPONENTS
    Support
  LINK_LIBS
    lldbCore
    lldbTarget
  )