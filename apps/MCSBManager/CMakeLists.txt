set(ManagerSources Manager.cc ManagerParams.cc ClientProxy.cc ShmMapper.cc
	SocketDaemon.cc SlabManager.cc GroupManager.cc ProxySlabTracker.cc
	DropReporter.cc SlabRequestManager.cc)

set(ClientZSources RunClientZ.cc ClientZ.cc)

include_directories(.) # so includes are of the form "MCSB/foo.h"

add_library(MCSBManager-lib ${ManagerSources}) # used by test apps
target_link_libraries(MCSBManager-lib MCSB)

add_library(MCSB-ClientZ ${ClientZSources})
target_link_libraries(MCSB-ClientZ MCSB)

add_executable(MCSBManager main.cc)
target_link_libraries(MCSBManager MCSB MCSBManager-lib MCSB-ClientZ ${MCSB_EXT_LIBS})

# for testing, not installed
add_executable(ClientZ main_ClientZ.cc)
target_link_libraries(ClientZ MCSB MCSB-ClientZ MCSBManager-lib ${MCSB_EXT_LIBS})

install(TARGETS MCSBManager RUNTIME DESTINATION ${INSTALL_BIN_DIR})
