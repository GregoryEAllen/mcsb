find_program(PRINTF_EXE printf)
mark_as_advanced(PRINTF_EXE)
math(EXPR MCSB_VERSION_DEC "${MCSB_MAJOR_VERSION}*256*256 + ${MCSB_MINOR_VERSION}*256 + ${MCSB_PATCH_VERSION}")

if(PRINTF_EXE)
	execute_process(
	COMMAND ${PRINTF_EXE} "0x%06X" ${MCSB_VERSION_DEC}
	OUTPUT_VARIABLE MCSB_VERSION_HEX
	OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

configure_file(MCSB/MCSBVersion.h.in ${CMAKE_CURRENT_BINARY_DIR}/MCSB/MCSBVersion.h)

# public headers
set(MCSB_INSTALL_HEADERS MCSB/BaseClient.h MCSB/Client.h MCSB/ClientCallbacks.h
	MCSB/dbprinter.h MCSB/ClientOptions.h MCSB/ClientWatcher.h
	MCSB/MessageDescriptors.h)
if(MCSB_ENABLE_LIBVARIANT)
	set(MCSB_INSTALL_HEADERS ${MCSB_INSTALL_HEADERS} MCSB/VariantClientOptions.h)
endif()

# copy to binary for doxygen to have them all in one place
foreach(file ${MCSB_INSTALL_HEADERS})
	configure_file(${file} ${CMAKE_CURRENT_BINARY_DIR}/${file} COPYONLY)
endforeach()

# add version for installation
set(MCSB_INSTALL_HEADERS ${MCSB_INSTALL_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/MCSB/MCSBVersion.h)
install(FILES ${MCSB_INSTALL_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/MCSB)
