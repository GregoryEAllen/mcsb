# Allow override of the default installation directories
set(INSTALL_LIB_DIR lib CACHE STRING "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE STRING "Installation directory for executable files")
set(INSTALL_INCLUDE_DIR include CACHE STRING "Installation directory for include files")

# Make relative paths absolute
foreach(p LIB BIN INCLUDE)
	set(var INSTALL_${p}_DIR)
	if(NOT IS_ABSOLUTE "${${var}}")
		set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
	endif()
endforeach()
