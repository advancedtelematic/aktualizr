if(BUILD_SOTA_TOOLS)
    add_aktualizr_test(NAME sota_tools_auth_test SOURCES auth_test.cc
        PROJECT_WORKING_DIRECTORY NO_VALGRIND)
    target_link_libraries(t_sota_tools_auth_test sota_tools_static_lib aktualizr_static_lib ${TEST_LIBS})
    target_include_directories(t_sota_tools_auth_test
        PUBLIC ${PROJECT_SOURCE_DIR}/src/sota_tools ${GLIB2_INCLUDE_DIRS})

    add_aktualizr_test(NAME ostree_hash SOURCES ostree_hash_test.cc)
    target_link_libraries(t_ostree_hash sota_tools_static_lib)

    add_aktualizr_test(NAME rate_controller SOURCES rate_controller_test.cc)
    target_link_libraries(t_rate_controller
        sota_tools_static_lib
        ${Boost_LIBRARIES})
endif(BUILD_SOTA_TOOLS)
