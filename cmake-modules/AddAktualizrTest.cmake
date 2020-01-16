function(add_aktualizr_test)
    set(options PROJECT_WORKING_DIRECTORY NO_VALGRIND)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES LIBRARIES ARGS LAUNCH_CMD)
    cmake_parse_arguments(AKTUALIZR_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(TEST_TARGET t_${AKTUALIZR_TEST_NAME})

    add_executable(${TEST_TARGET} EXCLUDE_FROM_ALL ${AKTUALIZR_TEST_SOURCES})
    target_link_libraries(${TEST_TARGET}
        ${AKTUALIZR_TEST_LIBRARIES}
        ${TEST_LIBS})
    target_include_directories(${TEST_TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/tests)

    if(AKTUALIZR_TEST_PROJECT_WORKING_DIRECTORY)
        set(WD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    else()
        set(WD )
    endif()

    if(TESTSUITE_VALGRIND AND (NOT AKTUALIZR_TEST_NO_VALGRIND))
        list(INSERT CMD_PREFIX 0 ${RUN_VALGRIND})
    endif()

    if(AKTUALIZR_TEST_LAUNCH_CMD)
        list(INSERT CMD_PREFIX 0 ${AKTUALIZR_TEST_LAUNCH_CMD})
    endif()

    add_test(NAME test_${AKTUALIZR_TEST_NAME}
             COMMAND ${CMD_PREFIX} $<TARGET_FILE:${TEST_TARGET}> ${AKTUALIZR_TEST_ARGS} ${GOOGLE_TEST_OUTPUT} ${WD})

    add_dependencies(build_tests ${TEST_TARGET})
    set(TEST_SOURCES ${TEST_SOURCES} ${AKTUALIZR_TEST_SOURCES} PARENT_SCOPE)
endfunction(add_aktualizr_test)
