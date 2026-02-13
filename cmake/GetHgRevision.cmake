find_program(HG_EXECUTABLE hg DOC "mercurial command line tool")
mark_as_advanced(HG_EXECUTABLE)

if(NOT HG_EXECUTABLE)
	set(HG_EXECUTABLE true)
endif()

# make PROJECT_NAME safe for C identifiers (locally only)
string(REGEX REPLACE "[^a-zA-Z0-9]" "_" PROJECT_NAME ${PROJECT_NAME})

set(HG_REV_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_HgRevision.c)
set(HG_REV_CMAKE  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_HgRevision.cmake)
set(${PROJECT_NAME}_HgRevision_SOURCE ${HG_REV_SOURCE})

file(WRITE ${HG_REV_SOURCE}.in
	"const char* kHgRevision_${PROJECT_NAME} = \"@HG_REVISION@\";\n")

# Generate HgRevision.cmake, which uses hg to create HgRevision.c
# configure_file() will only touch its output (HgRevision.c) if
# there would be a change, so make doesn't rebuild every time
file(WRITE ${HG_REV_CMAKE}
"execute_process(
	COMMAND ${HG_EXECUTABLE} id --debug -i
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	OUTPUT_VARIABLE HG_REVISION
	ERROR_QUIET
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
configure_file(${HG_REV_SOURCE}.in ${HG_REV_SOURCE} @ONLY)")

# this target regenerates HgRevision.c (by calling HgRevision.cmake)
add_custom_target(${PROJECT_NAME}_HgRevision
	${CMAKE_COMMAND} -P ${HG_REV_CMAKE})

# HgRevision.c is needed at cmake time, so
# we generate it immediately by including HgRevision.cmake
include(${HG_REV_CMAKE})
