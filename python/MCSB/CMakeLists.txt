FIND_PACKAGE(SWIG REQUIRED)
INCLUDE(${SWIG_USE_FILE})

find_package(PythonLibs 2.4)
include_directories(${PYTHON_INCLUDE_PATH})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_SWIG_FLAGS "")

set_source_files_properties(MCSB.i PROPERTIES CPLUSPLUS ON)

if(RT_LIBRARY)
	set(PyMCSB_EXT_LIBS ${RT_LIBRARY})
endif()

swig_add_module(MCSB python MCSB.i ${PyMCSB-Sources})
swig_link_libraries(MCSB ${PYTHON_LIBRARIES} ${PyMCSB_EXT_LIBS})

# copy to the BINARY_DIR so there is a complete MCSB python module there
set(PyInstallFiles __init__.py __main__.py ClientOptionParser.py)
foreach(file ${PyInstallFiles})
	configure_file(${file} ${CMAKE_CURRENT_BINARY_DIR}/${file} COPYONLY)
endforeach()

# installation: what
set(PyInstallFiles ${PyInstallFiles}
	${CMAKE_CURRENT_BINARY_DIR}/MCSB.py ${CMAKE_CURRENT_BINARY_DIR}/_MCSB.so)

# installation: where
execute_process(
	COMMAND python -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(plat_specific=True, prefix=''))"
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	OUTPUT_VARIABLE PyDestination
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
set(INSTALL_PYTHON_DIR ${PyDestination} CACHE PATH "Installation directory for python modules")
install(FILES ${PyInstallFiles} DESTINATION ${INSTALL_PYTHON_DIR}/MCSB/)
