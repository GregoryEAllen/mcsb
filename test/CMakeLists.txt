include_directories(../src)
include_directories(../apps/MCSBManager)

find_package(Threads)

add_library(MCSB-test ClientTester.cc rand_buf.cc RandomClient.cc)

add_executable(test_crc32c test_crc32c.cc)
target_link_libraries(test_crc32c MCSB ${MCSB_EXT_LIBS})
add_test(test_crc32c ${CMAKE_CURRENT_BINARY_DIR}/test_crc32c)

add_executable(test_ShmMapper test_ShmMapper.cc rand_buf.cc)
target_link_libraries(test_ShmMapper MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_ShmMapper ${CMAKE_CURRENT_BINARY_DIR}/test_ShmMapper)

add_executable(test_SocketDaemon test_SocketDaemon.cc)
target_link_libraries(test_SocketDaemon MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_SocketDaemon ${CMAKE_CURRENT_BINARY_DIR}/test_SocketDaemon)

add_executable(test_SocketEndpoint test_SocketEndpoint.cc)
target_link_libraries(test_SocketEndpoint MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_SocketEndpoint ${CMAKE_CURRENT_BINARY_DIR}/test_SocketEndpoint)

add_executable(test_IntrusiveList test_IntrusiveList.cc)
target_link_libraries(test_IntrusiveList MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_IntrusiveList ${CMAKE_CURRENT_BINARY_DIR}/test_IntrusiveList)

add_executable(test_SlabManager test_SlabManager.cc)
target_link_libraries(test_SlabManager MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_SlabManager ${CMAKE_CURRENT_BINARY_DIR}/test_SlabManager)

add_executable(test_SlabRequestManager test_SlabRequestManager.cc)
target_link_libraries(test_SlabRequestManager MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_SlabRequestManager ${CMAKE_CURRENT_BINARY_DIR}/test_SlabRequestManager)

add_executable(test_GroupManager test_GroupManager.cc)
target_link_libraries(test_GroupManager MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_GroupManager ${CMAKE_CURRENT_BINARY_DIR}/test_GroupManager)

add_executable(test_ClientOptions test_ClientOptions.cc)
target_link_libraries(test_ClientOptions MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_ClientOptions ${CMAKE_CURRENT_BINARY_DIR}/test_ClientOptions)

add_executable(test_ClientSendManager test_ClientSendManager.cc)
target_link_libraries(test_ClientSendManager MCSB ${MCSB_EXT_LIBS})
add_test(test_ClientSendManager ${CMAKE_CURRENT_BINARY_DIR}/test_ClientSendManager)

add_executable(test_ClientRecvManager test_ClientRecvManager.cc)
target_link_libraries(test_ClientRecvManager MCSB ${MCSB_EXT_LIBS})
add_test(test_ClientRecvManager ${CMAKE_CURRENT_BINARY_DIR}/test_ClientRecvManager)

add_executable(test_ClientImpl test_ClientImpl.cc rand_buf.cc)
target_link_libraries(test_ClientImpl MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_ClientImpl ${CMAKE_CURRENT_BINARY_DIR}/test_ClientImpl)

add_executable(test_ClientImplRand test_ClientImplRand.cc rand_buf.cc)
target_link_libraries(test_ClientImplRand MCSB MCSBManager-lib ${MCSB_EXT_LIBS}
	${CMAKE_THREAD_LIBS_INIT})
add_test(test_ClientImplRand ${CMAKE_CURRENT_BINARY_DIR}/test_ClientImplRand)

add_executable(test_Client test_Client.cc ClientTester.cc)
target_link_libraries(test_Client MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_Client ${CMAKE_CURRENT_BINARY_DIR}/test_Client)

add_executable(test_Groups test_Groups.cc ClientTester.cc)
target_link_libraries(test_Groups MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_Groups ${CMAKE_CURRENT_BINARY_DIR}/test_Groups)

add_executable(test_ClientWatcher test_ClientWatcher.cc ClientTester.cc)
target_link_libraries(test_ClientWatcher MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_ClientWatcher ${CMAKE_CURRENT_BINARY_DIR}/test_ClientWatcher)

add_executable(test_RandomClient test_RandomClient.cc ClientTester.cc rand_buf.cc RandomClient.cc)
target_link_libraries(test_RandomClient MCSB MCSBManager-lib ${MCSB_EXT_LIBS})
add_test(test_RandomClient ${CMAKE_CURRENT_BINARY_DIR}/test_RandomClient)

add_executable(test_SlowClient test_SlowClient.cc)
target_link_libraries(test_SlowClient MCSB MCSBManager-lib ${MCSB_EXT_LIBS}
	${CMAKE_THREAD_LIBS_INIT})
#add_test(test_SlowClient ${CMAKE_CURRENT_BINARY_DIR}/test_SlowClient)

find_program(PYTHON2_EXECUTABLE python2 DOC "python interpreter, version 2.x")
mark_as_advanced(PYTHON2_EXECUTABLE)
if(NOT PYTHON2_EXECUTABLE)
	set(PYTHON2_EXECUTABLE python)
endif()

if(MCSB_BUILD_PYTHON)
	get_property(MCSBManager_Location TARGET MCSBManager PROPERTY LOCATION)
	get_property(_MCSB_Location TARGET _MCSB PROPERTY LOCATION)
	configure_file(PythonPaths.py.in ${CMAKE_CURRENT_BINARY_DIR}/PythonPaths.py)
	configure_file(test_PyInterface.py ${CMAKE_CURRENT_BINARY_DIR}/test_PyInterface.py COPYONLY)
	add_test(test_PyInterface ${PYTHON2_EXECUTABLE} test_PyInterface.py)
endif()

if(MCSB_ENABLE_LIBVARIANT)
	add_executable(test_VariantClientOptions test_VariantClientOptions.cc)
	target_link_libraries(test_VariantClientOptions MCSB ${MCSB_EXT_LIBS})
	add_test(test_VariantClientOptions ${CMAKE_CURRENT_BINARY_DIR}/test_VariantClientOptions)
endif()
