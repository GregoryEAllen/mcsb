include(GetHgRevision)

set(MCSB-Sources ShmDefs.cc ShmClient.cc SocketClient.cc SocketEndpoint.cc
	ClientOptions.cc ClientImpl.cc ClientSendManager.cc ClientRecvManager.cc
	TestingClientOptions.cc MessageSegment.cc MessageDescriptors.cc
	dbprinter.cc uptimer.cc crc32c.cc BaseClient.cc Client.cc
	${MCSB_HgRevision_SOURCE})

set(PyMCSB-Sources ${MCSB-Sources}) # sources after this line not in python

set(MCSB-Sources ${MCSB-Sources} ClientWatcher.cc)

if(MCSB_ENABLE_LIBVARIANT)
	set(MCSB-Sources ${MCSB-Sources} VariantClientOptions.cc)
endif()

add_library(MCSB ${MCSB-Sources})
add_dependencies(MCSB MCSB_HgRevision)
target_link_libraries(MCSB ${EXTRA_LIBS})

install(TARGETS MCSB DESTINATION ${INSTALL_LIB_DIR})

# get PyMCSB-Sources with full paths into the parent scope
foreach(FILE ${PyMCSB-Sources})
	if (IS_ABSOLUTE ${FILE})
		set(PyMCSB-Sources-Full ${PyMCSB-Sources-Full} ${FILE})
	else()
		set(PyMCSB-Sources-Full ${PyMCSB-Sources-Full} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE})
	endif()
endforeach()
set(PyMCSB-Sources ${PyMCSB-Sources-Full} PARENT_SCOPE)
