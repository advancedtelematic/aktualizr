find_program(VALGRIND NAMES valgrind)

if(NOT GTEST_ROOT )
    set(GTEST_ROOT ${PROJECT_SOURCE_DIR}/third_party/googletest/googletest)
endif()

if(NOT GMOCK_ROOT )
    set(GMOCK_ROOT ${PROJECT_SOURCE_DIR}/third_party/googletest/googlemock)
endif()
add_subdirectory(${GMOCK_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gmock EXCLUDE_FROM_ALL)

# use the same libraries as the normal target but add gtest gmock and gcov
set (TEST_LIBS
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${SODIUM_LIBRARIES}
    ${LIBOSTREE_LIBRARIES}
    ${LIBDBUS_LIBRARIES}
    ${LibArchive_LIBRARIES}
    gtest gmock gcov)

if(BUILD_GENIVI)
    list(APPEND TEST_LIBS rvi)
endif(BUILD_GENIVI)

add_custom_target(build_tests DEPENDS aktualizr)
add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=1 ${CMAKE_CTEST_COMMAND} -E test_valgrind_uptane_vectors\\|test_build DEPENDS build_tests)
add_custom_target(check-full COMMAND CTEST_OUTPUT_ON_FAILURE=1 ${CMAKE_CTEST_COMMAND} -E test_uptane_vectors DEPENDS build_tests)

include(CMakeParseArguments)

function(add_aktualizr_test)
    set(options PROJECT_WORKING_DIRECTORY NO_VALGRIND)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES ARGS)
    cmake_parse_arguments(AKTUALIZR_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    add_executable(t_${AKTUALIZR_TEST_NAME} ${AKTUALIZR_TEST_SOURCES})
    target_link_libraries(t_${AKTUALIZR_TEST_NAME} aktualizr_static_lib ${TEST_LIBS})
    if(AKTUALIZR_TEST_PROJECT_WORKING_DIRECTORY)
        set(WD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    else()
        set(WD )
    endif()
        # Running tests that are both instrumented by gcov and valgrind at
        # the same time is very expensive.  Disable valgrind when running
        # under gcov, or when the test is explicitly flagged that it fails
        # under valgrind (these should be fixed)
        if(AKTUALIZR_TEST_NO_VALGRIND OR BUILD_WITH_CODE_COVERAGE)
            add_test(NAME test_${AKTUALIZR_TEST_NAME}
                    COMMAND t_${AKTUALIZR_TEST_NAME} ${AKTUALIZR_TEST_ARGS}
                    ${WD})
        else()
            add_test(NAME test_${AKTUALIZR_TEST_NAME}
                     COMMAND ${RUN_VALGRIND} ${CMAKE_CURRENT_BINARY_DIR}/t_${AKTUALIZR_TEST_NAME} ${AKTUALIZR_TEST_ARGS}
                     ${WD})
        endif()
    add_dependencies(build_tests t_${AKTUALIZR_TEST_NAME})

endfunction(add_aktualizr_test)

# Setup coverage
if(BUILD_WITH_CODE_COVERAGE)
    include(CodeCoverage)
    setup_target_for_coverage(coverage ctest coverage)
    add_definitions(-g -O0 -fprofile-arcs -ftest-coverage --coverage )
    target_compile_options(aktualizr_static_lib PUBLIC  -fprofile-arcs -ftest-coverage)
    add_dependencies(coverage build_tests)
endif(BUILD_WITH_CODE_COVERAGE)

configure_file(run-valgrind.in run-valgrind @ONLY)
SET(RUN_VALGRIND ${CMAKE_CURRENT_BINARY_DIR}/run-valgrind)


add_aktualizr_test(NAME config SOURCES config_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME events SOURCES events_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME commands SOURCES commands_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME http_client
                   SOURCES httpclient_test.cc
                   ARGS ${PROJECT_SOURCE_DIR}/tests/fake_http_server/
                   PROJECT_WORKING_DIRECTORY)

if(NOT BUILD_WITH_CODE_COVERAGE)
    # Code coverage disables valgrind, and this test is only checking that
    # valgrind finds memory leaks
    add_aktualizr_test(NAME leak SOURCES leak_test.cc)
    set_tests_properties(test_leak PROPERTIES WILL_FAIL TRUE)
endif()

add_aktualizr_test(NAME socket_gateway
                   SOURCES socketgateway_test.cc
                   ARGS ${PROJECT_SOURCE_DIR}/tests/fake_unix_socket/)

add_aktualizr_test(NAME timer SOURCES timer_test.cc)

add_aktualizr_test(NAME utils SOURCES utils_test.cc)

add_aktualizr_test(NAME fsstorage SOURCES fsstorage_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME crypto
                   SOURCES crypto_test.cc
                   PROJECT_WORKING_DIRECTORY NO_VALGRIND)

if(BUILD_OSTREE)
    set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS -w)

    add_aktualizr_test(NAME uptane SOURCES uptane_test.cc fake_ostree.cc
                       PROJECT_WORKING_DIRECTORY NO_VALGRIND)

    add_aktualizr_test(NAME ostree SOURCES ostree_test.cc PROJECT_WORKING_DIRECTORY NO_VALGRIND)

    add_aktualizr_test(NAME tuf SOURCES tuf_test.cc PROJECT_WORKING_DIRECTORY)

    add_executable(aktualizr_uptane_vector_tests uptane_vector_tests.cc fake_ostree.cc)
    target_link_libraries(aktualizr_uptane_vector_tests aktualizr_static_lib ${TEST_LIBS} crypto)

    add_test(NAME test_uptane_vectors COMMAND ${PROJECT_SOURCE_DIR}/tests/run_vector_tests.sh ${PROJECT_SOURCE_DIR}/tests/tuf-test-vectors)
    add_test(NAME test_valgrind_uptane_vectors COMMAND ${PROJECT_SOURCE_DIR}/tests/run_vector_tests.sh ${PROJECT_SOURCE_DIR}/tests/tuf-test-vectors valgrind)
    add_dependencies(build_tests aktualizr_uptane_vector_tests)

    add_test(NAME test_ostree_invalid
        COMMAND aktualizr --config ${CMAKE_CURRENT_SOURCE_DIR}/missing_ostree_repo.toml)
    set_tests_properties(test_ostree_invalid PROPERTIES PASS_REGULAR_EXPRESSION "Could not find OSTree sysroot at:")

    add_aktualizr_test(NAME uptane_http_fake SOURCES uptane_fake_http_test.cc PROJECT_WORKING_DIRECTORY NO_VALGRIND)


    if(SOTA_PACKED_CREDENTIALS)
        add_aktualizr_test(NAME uptane_ci SOURCES uptane_ci_test.cc fake_ostree.cc)
    endif(SOTA_PACKED_CREDENTIALS)

endif(BUILD_OSTREE)


if(BUILD_GENIVI)
    add_aktualizr_test(NAME rvi_client SOURCES rvisotaclient_test.cc PROJECT_WORKING_DIRECTORY)

    add_executable(aktualizr_test_dbusgateway dbusgateway_test.cc)
    target_link_libraries(aktualizr_test_dbusgateway aktualizr_static_lib ${TEST_LIBS} ${LIBDBUS_LIBRARIES})
    add_test(NAME test_dbusgateway
             COMMAND dbus-run-session --config-file ${PROJECT_SOURCE_DIR}/tests/session.conf -- ${CMAKE_CURRENT_BINARY_DIR}/aktualizr_test_dbusgateway ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(aktualizr_test_swm swm_test.cc)
    target_link_libraries(aktualizr_test_swm aktualizr_static_lib ${TEST_LIBS} ${LIBDBUS_LIBRARIES})
    add_test(NAME test_swm
             COMMAND dbus-run-session --config-file ${PROJECT_SOURCE_DIR}/tests/session.conf -- ${CMAKE_CURRENT_BINARY_DIR}/aktualizr_test_swm ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    add_dependencies(build_tests aktualizr_test_dbusgateway aktualizr_test_swm)

endif(BUILD_GENIVI)


###############################################################################
# The test feature of cmake checks the return value when the program
# exits. If the return value is zero, the testcase passes.

# test running the executable with command line option --help
add_test(NAME feat1_test--help
         COMMAND aktualizr --help)
# test running the executable with command line option -h
add_test(NAME feat1_test-h
         COMMAND aktualizr -h)
# test running the executable with command line option --something
add_test(NAME feat1_test--something
         COMMAND aktualizr --something -c ${PROJECT_SOURCE_DIR}/config/config.toml.example)
# test running the executable with command line option -sth
add_test(NAME feat1_test-sth
         COMMAND aktualizr -sth -c ${PROJECT_SOURCE_DIR}/config/config.toml.example)

# calle the executable without any options
add_test(NAME test_plain
         COMMAND aktualizr)

# check invalid logging level

add_test(NAME test_log_invalid
         COMMAND aktualizr --loglevel=5)


# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass
set_tests_properties(feat1_test--something
                     feat1_test-sth
                     test_plain
                     test_log_invalid
                     PROPERTIES WILL_FAIL TRUE)

# test the return code when running the executable with non-existing configuration file
add_test(NAME test-no-config-check-code
         COMMAND aktualizr -c non-existing-config.toml)

set_tests_properties(test-no-config-check-code
                     PROPERTIES WILL_FAIL TRUE)

# test the error message when running the executable with non-existing configuration file
add_test(NAME test-no-config-check-message
         COMMAND aktualizr -c non-existing-config.toml)

set_tests_properties(test-no-config-check-message
                     PROPERTIES PASS_REGULAR_EXPRESSION  "aktualizr: configuration file .* not found. Exiting.")

# Try building with various cmake options
add_test(NAME test_build_all_off
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options.sh ${PROJECT_SOURCE_DIR} test_build_all_off "-DBUILD_GENIVI=OFF -DBUILD_OSTREE=OFF")

add_test(NAME test_build_all_on
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options.sh ${PROJECT_SOURCE_DIR} test_build_all_on "-DBUILD_WITH_CODE_COVERAGE=ON -DBUILD_GENIVI=ON -DBUILD_OSTREE=ON")

add_test(NAME test_build_debug
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options.sh ${PROJECT_SOURCE_DIR} test_build_debug "-DCMAKE_BUILD_TYPE=Debug")

add_test(NAME test_build_release
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options.sh ${PROJECT_SOURCE_DIR} test_build_release "-DCMAKE_BUILD_TYPE=Release")

add_dependencies(qa check-full)


set(ALL_TEST_SRCS
    commands_test.cc
    config_test.cc
    crypto_test.cc
    dbusgateway_test.cc
    events_test.cc
    httpclient_test.cc
    ostree_test.cc
    rvisotaclient_test.cc
    socketgateway_test.cc
    swm_test.cc
    timer_test.cc
    tuf_test.cc
    uptane_vector_tests.cc
    uptane_test.cc
    uptane_ci_test.cc
    fake_ostree.cc
    utils_test.cc
    uptane_fake_http_test.cc)

add_custom_target(check-format-tests
    COMMAND ../scripts/check-formatting.sh ${CLANG_FORMAT} ${ALL_TEST_SRCS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking clang-format on tests"
    VERBATIM)
add_dependencies(check-format check-format-tests)

if(CLANG_FORMAT)
    add_custom_target(format-tests
        COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_TEST_SRCS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on tests"
        VERBATIM)
    add_dependencies(qa format-tests)
else()
    message(WARNING "clang-format not found, skipping")
endif()

# vim: set tabstop=4 shiftwidth=4 expandtab:
