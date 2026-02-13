find_path(LIBEV_INCLUDE_DIRS ev.h ev++.h /usr/include/libev)
find_library(LIBEV_LIBRARIES NAMES ev)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibEV DEFAULT_MSG LIBEV_LIBRARIES LIBEV_INCLUDE_DIRS)

