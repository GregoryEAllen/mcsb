include_directories(../../src)

add_executable(mcsb_source mcsb_source.cc)
target_link_libraries(mcsb_source MCSB ${MCSB_EXT_LIBS})

add_executable(mcsb_regs mcsb_regs.cc)
target_link_libraries(mcsb_regs MCSB ${MCSB_EXT_LIBS})

if(NOT MCSB_ENABLE_LIBVARIANT)
	return()
endif()

add_executable(mcsb_sink mcsb_sink.cc)
target_link_libraries(mcsb_sink MCSB ${MCSB_EXT_LIBS})

add_executable(mcsb_blast mcsb_blast.cc)
target_link_libraries(mcsb_blast MCSB ${MCSB_EXT_LIBS})

add_executable(mcsb_ping mcsb_ping.cc)
target_link_libraries(mcsb_ping MCSB ${MCSB_EXT_LIBS})

install(TARGETS mcsb_source mcsb_regs mcsb_sink mcsb_blast mcsb_ping
	RUNTIME DESTINATION ${INSTALL_BIN_DIR})
