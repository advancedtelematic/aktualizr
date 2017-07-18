
if(NOT GTEST_ROOT )
    set(GTEST_ROOT ${PROJECT_SOURCE_DIR}/third_party/googletest/googletest)
endif()

if(NOT GMOCK_ROOT )
    set(GMOCK_ROOT ${PROJECT_SOURCE_DIR}/third_party/googletest/googlemock)
endif()
add_subdirectory(${GMOCK_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gmock EXCLUDE_FROM_ALL)

# use the same libiraries as the normal target but add gtest gmock and gcov
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
add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=1 ${CMAKE_CTEST_COMMAND} DEPENDS build_tests)

# Setup coverage
if(BUILD_WITH_CODE_COVERAGE)
    include(CodeCoverage)
    setup_target_for_coverage(coverage ctest coverage)
    add_definitions(-g -O0 -fprofile-arcs -ftest-coverage --coverage )
    target_compile_options(aktualizr_static_lib PUBLIC  -fprofile-arcs -ftest-coverage)
    add_dependencies(coverage build_tests)
endif(BUILD_WITH_CODE_COVERAGE)

# Config Test
add_executable(aktualizr_test_config config_test.cc)
target_link_libraries(aktualizr_test_config aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_config COMMAND aktualizr_test_config WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


add_executable(aktualizr_test_events events_test.cc)
target_link_libraries(aktualizr_test_events aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_events COMMAND aktualizr_test_events WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


add_executable(aktualizr_test_commands commands_test.cc)
target_link_libraries(aktualizr_test_commands aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_commands COMMAND aktualizr_test_commands WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


add_executable(aktualizr_test_http_aktualizr httpsotaclient_test.cc)
target_link_libraries(aktualizr_test_http_aktualizr aktualizr_static_lib ${TEST_LIBS} crypto)
add_test(NAME test_http_aktualizr COMMAND aktualizr_test_http_aktualizr WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


add_executable(aktualizr_test_http_client httpclient_test.cc)
target_link_libraries(aktualizr_test_http_client aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_http_client COMMAND aktualizr_test_http_client ${PROJECT_SOURCE_DIR}/tests/fake_http_server/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
add_test(NAME test_valgrind_http_client COMMAND valgrind --track-origins=yes --leak-check=full --error-exitcode=1 --suppressions=${PROJECT_SOURCE_DIR}/tests/aktualizr.supp ${CMAKE_CURRENT_BINARY_DIR}/aktualizr_test_http_client ${PROJECT_SOURCE_DIR}/tests/fake_http_server/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(aktualizr_test_socket_gateway socketgateway_test.cc)
target_link_libraries(aktualizr_test_socket_gateway aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_socket_gateway COMMAND aktualizr_test_socket_gateway ${PROJECT_SOURCE_DIR}/tests/fake_unix_socket/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
add_test(NAME test_valgrind_socket_gateway COMMAND valgrind --track-origins=yes --leak-check=full --error-exitcode=1 --suppressions=${PROJECT_SOURCE_DIR}/tests/aktualizr.supp ${CMAKE_CURRENT_BINARY_DIR}/aktualizr_test_socket_gateway ${PROJECT_SOURCE_DIR}/tests/fake_unix_socket/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(aktualizr_test_crypto crypto_test.cc)
target_link_libraries(aktualizr_test_crypto aktualizr_static_lib ${TEST_LIBS} crypto ${SODIUM_LIBRARIES})
add_test(NAME test_crypto COMMAND aktualizr_test_crypto WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(aktualizr_test_utils utils_test.cc)
target_link_libraries(aktualizr_test_utils aktualizr_static_lib ${TEST_LIBS} crypto ${SODIUM_LIBRARIES})
add_test(NAME test_utils COMMAND valgrind --track-origins=yes --leak-check=full --error-exitcode=1 --suppressions=${PROJECT_SOURCE_DIR}/tests/aktualizr.supp ${CMAKE_CURRENT_BINARY_DIR}/aktualizr_test_utils)


add_dependencies(build_tests
        aktualizr_test_config
        aktualizr_test_events
        aktualizr_test_commands
        aktualizr_test_http_aktualizr
        aktualizr_test_http_client
        aktualizr_test_socket_gateway
        aktualizr_test_crypto
        aktualizr_test_utils)

if(BUILD_OSTREE)
    add_executable(aktualizr_test_uptane
                  ${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp
                  ${PROJECT_SOURCE_DIR}/src/commands.cc
                  ${PROJECT_SOURCE_DIR}/src/config.cc
                  ${PROJECT_SOURCE_DIR}/src/events.cc
                  ${PROJECT_SOURCE_DIR}/src/sotauptaneclient.cc
                  ${PROJECT_SOURCE_DIR}/src/logger.cc
                  ${PROJECT_SOURCE_DIR}/src/crypto.cc
                  ${PROJECT_SOURCE_DIR}/src/types.cc
                  ${PROJECT_SOURCE_DIR}/src/uptane/role.cc
                  ${PROJECT_SOURCE_DIR}/src/uptane/root.cc
                  ${PROJECT_SOURCE_DIR}/src/uptane/tuf.cc
                  ${PROJECT_SOURCE_DIR}/src/uptane/tufrepository.cc
                  ${PROJECT_SOURCE_DIR}/src/uptane/uptanerepository.cc
                  ${PROJECT_SOURCE_DIR}/src/uptane/secondary.cc
                  ${PROJECT_SOURCE_DIR}/src/uptane/testbusprimary.cc
                  ${PROJECT_SOURCE_DIR}/src/uptane/testbussecondary.cc
                  ${PROJECT_SOURCE_DIR}/src/utils.cc
                  uptane_test.cc)
    target_link_libraries(aktualizr_test_uptane ${TEST_LIBS} crypto ${SODIUM_LIBRARIES})
    add_test(NAME test_uptane COMMAND  valgrind --error-exitcode=1 --track-origins=yes --suppressions=${PROJECT_SOURCE_DIR}/tests/aktualizr.supp ${CMAKE_CURRENT_BINARY_DIR}/aktualizr_test_uptane WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(aktualizr_test_ostree ostree_test.cc)
    target_link_libraries(aktualizr_test_ostree aktualizr_static_lib ${TEST_LIBS} crypto)
    add_test(NAME test_ostree COMMAND aktualizr_test_ostree WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(aktualizr_tuf_tests tuf_test.cc)
    target_link_libraries(aktualizr_tuf_tests aktualizr_static_lib ${TEST_LIBS} crypto ${SODIUM_LIBRARIES})
    add_test(NAME test_tuf COMMAND valgrind --error-exitcode=1 --suppressions=${PROJECT_SOURCE_DIR}/tests/aktualizr.supp ${CMAKE_CURRENT_BINARY_DIR}/aktualizr_tuf_tests WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(aktualizr_uptane_vector_tests uptane_vector_tests.cc)
    target_link_libraries(aktualizr_uptane_vector_tests aktualizr_static_lib ${TEST_LIBS} crypto ${SODIUM_LIBRARIES})
    add_test(NAME test_uptane_vectors COMMAND ${PROJECT_SOURCE_DIR}/tests/run_vector_tests.sh ${PROJECT_SOURCE_DIR}/tests/tuf-test-vectors)

    add_test(NAME test_valgrind_uptane_vectors COMMAND ${PROJECT_SOURCE_DIR}/tests/run_vector_tests.sh ${PROJECT_SOURCE_DIR}/tests/tuf-test-vectors valgrind)

    add_dependencies(build_tests aktualizr_test_uptane aktualizr_test_ostree aktualizr_tuf_tests aktualizr_uptane_vector_tests)
endif(BUILD_OSTREE)


if(BUILD_GENIVI)
    add_executable(aktualizr_test_dbusgateway dbusgateway_test.cc)
    target_link_libraries(aktualizr_test_dbusgateway aktualizr_static_lib ${TEST_LIBS} ${LIBDBUS_LIBRARIES})
    add_test(NAME test_dbusgateway COMMAND dbus-run-session --config-file ${PROJECT_SOURCE_DIR}/tests/session.conf bash -c "${CMAKE_CURRENT_BINARY_DIR}/aktualizr_test_dbusgateway ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    add_test(NAME test_valgrind_dbusgateway COMMAND valgrind --track-origins=yes --leak-check=full --error-exitcode=1 --suppressions=${PROJECT_SOURCE_DIR}/tests/aktualizr.supp dbus-run-session --config-file ${PROJECT_SOURCE_DIR}/tests/session.conf bash -c "${CMAKE_CURRENT_BINARY_DIR}/aktualizr_test_dbusgateway ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


    add_executable(aktualizr_test_rvi_client rvisotaclient_test.cc)
    target_link_libraries(aktualizr_test_rvi_client aktualizr_static_lib ${TEST_LIBS})
    add_test(NAME test_rvi_client COMMAND aktualizr_test_rvi_client WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(aktualizr_test_swm swm_test.cc)
    #target_compile_options(aktualizr_test_swm PUBLIC ${LIBDBUS_CFLAGS})
    target_link_libraries(aktualizr_test_swm aktualizr_static_lib ${TEST_LIBS} ${LIBDBUS_LIBRARIES})
    add_test(NAME test_swm COMMAND dbus-run-session --config-file ${PROJECT_SOURCE_DIR}/tests/session.conf bash -c "${CMAKE_CURRENT_BINARY_DIR}/aktualizr_test_swm ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    add_dependencies(build_tests aktualizr_test_dbusgateway aktualizr_test_rvi_client aktualizr_test_swm)

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

# Try building with various cmake options
add_test(NAME test_build_all_off
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options ${PROJECT_SOURCE_DIR} test_build_all_off "-DBUILD_GENIVI=OFF -DBUILD_TESTS=OFF")

add_test(NAME test_build_all_on
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options ${PROJECT_SOURCE_DIR} test_build_all_on "-DBUILD_WITH_CODE_COVERAGE=ON -DBUILD_GENIVI=ON -DBUILD_OSTREE=ON")

add_test(NAME test_build_debug
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options ${PROJECT_SOURCE_DIR} test_build_debug "-DCMAKE_BUILD_TYPE=Debug")

add_test(NAME test_build_release
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options ${PROJECT_SOURCE_DIR} test_build_release "-DCMAKE_BUILD_TYPE=Release")

add_dependencies(qa check)

set(ALL_TEST_SRCS
    commands_test.cc
    config_test.cc
    crypto_test.cc
    dbusgateway_test.cc
    events_test.cc
    httpclient_test.cc
    httpsotaclient_test.cc
    ostree_test.cc
    rvisotaclient_test.cc
    socketgateway_test.cc
    swm_test.cc
    tuf_test.cc
    uptane_vector_tests.cc
    uptane_test.cc
    utils_test.cc)

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
