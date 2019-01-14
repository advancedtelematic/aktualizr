function(add_aktualizr_test)
    set(options PROJECT_WORKING_DIRECTORY NO_VALGRIND)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES LIBRARIES ARGS)
    cmake_parse_arguments(AKTUALIZR_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    add_executable(t_${AKTUALIZR_TEST_NAME} EXCLUDE_FROM_ALL ${AKTUALIZR_TEST_SOURCES} ${PROJECT_SOURCE_DIR}/tests/test_utils.cc)
    target_link_libraries(t_${AKTUALIZR_TEST_NAME}
        ${AKTUALIZR_TEST_LIBRARIES}
        aktualizr_static_lib
        ${TEST_LIBS})
    target_include_directories(t_${AKTUALIZR_TEST_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/tests)

    if(AKTUALIZR_TEST_PROJECT_WORKING_DIRECTORY)
        set(WD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    else()
        set(WD )
    endif()

    # run tests under valgrind if the correct CMAKE_BUILD_TYPE is set
    if((CMAKE_BUILD_TYPE MATCHES "Valgrind") AND (NOT AKTUALIZR_TEST_NO_VALGRIND))
        add_test(NAME test_${AKTUALIZR_TEST_NAME}
                 COMMAND ${RUN_VALGRIND} ${CMAKE_CURRENT_BINARY_DIR}/t_${AKTUALIZR_TEST_NAME} ${AKTUALIZR_TEST_ARGS} ${GOOGLE_TEST_OUTPUT}
                 ${WD})
    else()
        add_test(NAME test_${AKTUALIZR_TEST_NAME}
                 COMMAND t_${AKTUALIZR_TEST_NAME} ${AKTUALIZR_TEST_ARGS} ${GOOGLE_TEST_OUTPUT}
                 ${WD})
    endif()

    add_dependencies(build_tests t_${AKTUALIZR_TEST_NAME})
    set(TEST_SOURCES ${TEST_SOURCES} ${AKTUALIZR_TEST_SOURCES} PARENT_SCOPE)
endfunction(add_aktualizr_test)
