add_custom_target(examples)

add_executable(ex1_SimpleHello EXCLUDE_FROM_ALL ex1_SimpleHello.cc)
target_link_libraries(ex1_SimpleHello MCSB ${MCSB_EXT_LIBS})

add_executable(ex2_DescriptorHello EXCLUDE_FROM_ALL ex2_DescriptorHello.cc)
target_link_libraries(ex2_DescriptorHello MCSB ${MCSB_EXT_LIBS})

add_executable(ex3_ClientWatcher EXCLUDE_FROM_ALL ex3_ClientWatcher.cc)
target_link_libraries(ex3_ClientWatcher MCSB ${MCSB_EXT_LIBS})

add_dependencies(examples ex1_SimpleHello ex2_DescriptorHello ex3_ClientWatcher)

if(NOT MCSB_ENABLE_LIBVARIANT)
	return()
endif()

add_executable(ex4_VariantClientOptions EXCLUDE_FROM_ALL ex4_VariantClientOptions.cc)
target_link_libraries(ex4_VariantClientOptions MCSB ${MCSB_EXT_LIBS})

add_dependencies(examples ex4_VariantClientOptions)
