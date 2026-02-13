find_package(Doxygen)
option(${PROJECT_NAME}_BUILD_DOCS "build MCSB with Doxygen documentation" ${DOXYGEN_FOUND})

if(${PROJECT_NAME}_BUILD_DOCS)
	if(NOT DOXYGEN_FOUND)
		message(FATAL_ERROR "Doxygen is required to build the documentation.")
	endif()
	set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in)
	set(DOXYFILE ${PROJECT_BINARY_DIR}/Doxyfile)

	configure_file(${DOXYFILE_IN} ${DOXYFILE} @ONLY IMMEDIATE)
	add_custom_target(doc
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE}
		SOURCES ${DOXYFILE})
endif()
