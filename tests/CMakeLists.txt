

if(NOT GTEST_ROOT )
    set(GTEST_ROOT /usr/src/gtest)
endif()

if(NOT GMOCK_ROOT )
    set(GMOCK_ROOT /usr/src/gmock)
endif()

add_subdirectory(${GMOCK_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gmock EXCLUDE_FROM_ALL)

# Setup warnings
if (CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-fstack-protector-all)
    # Enable maximum of Warnings :
    add_definitions(-Wall -Wextra -Wswitch-default -Wswitch -Winit-self -Wformat-security -Wfloat-equal -Wcast-qual -Wconversion -Wlogical-op)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "4.9" OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.9")
        add_definitions (-Wfloat-conversion)
        add_definitions (-Wshadow)
    endif ()

    if(WARNING_AS_ERROR)
        add_definitions (-Werror)
    endif()
endif()


# Export compile_commands.json for clang-check
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



set(ALL_TEST_SRCS tests/commands_test.cc
                  tests/config_test.cc
                  tests/dbusgateway_test.cc
                  tests/events_test.cc
                  tests/httpclient_test.cc
                  tests/httpsotaclient_test.cc
                  tests/rvisotaclient_test.cc
                  tests/socketgateway_test.cc
                  tests/swm_test.cc)

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

###############################################################################

# Workaround a cmake bug where tests are not dependent on their executables
# http://stackoverflow.com/questions/733475/cmake-ctest-make-test-doesnt-build-tests#736838
add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=1 ${CMAKE_CTEST_COMMAND}
  DEPENDS aktualizr_test_config aktualizr_test_commands aktualizr_test_events
        aktualizr_test_http_aktualizr aktualizr_test_http_client aktualizr_test_socket_gateway)

add_dependencies(qa check)

###############################################################################

#Setup CMake to run tests

#enable test features of cmake
enable_testing()

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/third_party/rvi_lib/include)

# use the same libiraries as the normal target but add gtest
set (TEST_LIBS ${Boost_LIBRARIES} ${CURL_LIBRARIES} gtest gmock gcov)
               

# Setup coverage
if(BUILD_WITH_CODE_COVERAGE)
    include(CodeCoverage)
    setup_target_for_coverage(coverage ctest coverage)
    add_definitions(-g -O0 -fprofile-arcs -ftest-coverage)
    target_link_libraries(aktualizr gcov)
endif()               


# Config Test
add_executable(aktualizr_test_config tests/config_test.cc)
target_link_libraries(aktualizr_test_config  aktualizr_static_lib ${TEST_LIBS})
file(WRITE Testing/config.toml "")
add_test(NAME test_config COMMAND aktualizr_test_config WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


add_executable(aktualizr_test_events tests/events_test.cc)
target_link_libraries(aktualizr_test_events aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_events COMMAND aktualizr_test_events WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


add_executable(aktualizr_test_commands tests/commands_test.cc)
target_link_libraries(aktualizr_test_commands aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_commands COMMAND aktualizr_test_commands WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


add_executable(aktualizr_test_http_aktualizr tests/httpsotaclient_test.cc)
target_link_libraries(aktualizr_test_http_aktualizr aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_http_aktualizr COMMAND aktualizr_test_http_aktualizr WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


add_executable(aktualizr_test_http_client tests/httpclient_test.cc)
target_link_libraries(aktualizr_test_http_client aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_http_client COMMAND aktualizr_test_http_client ${PROJECT_SOURCE_DIR}/tests/fake_http_server/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


add_executable(aktualizr_test_socket_gateway tests/socketgateway_test.cc)
target_link_libraries(aktualizr_test_socket_gateway aktualizr_static_lib ${TEST_LIBS})
add_test(NAME test_socket_gateway COMMAND aktualizr_test_socket_gateway ${PROJECT_SOURCE_DIR}/tests/fake_unix_socket/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

if(BUILD_GENIVI)
    add_executable(aktualizr_test_dbusgateway tests/dbusgateway_test.cc)
    target_compile_options(aktualizr_test_dbusgateway PUBLIC ${LIBDBUS_CFLAGS})
    target_link_libraries(aktualizr_test_dbusgateway aktualizr_static_lib ${TEST_LIBS} ${LIBDBUS_LIBRARIES})
    add_test(NAME test_dbusgateway COMMAND dbus-run-session --config-file ${PROJECT_SOURCE_DIR}/tests/session.conf bash -c "${CMAKE_BINARY_DIR}/aktualizr_test_dbusgateway ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    add_dependencies(check aktualizr_test_dbusgateway)

    add_executable(aktualizr_test_rvi_client tests/rvisotaclient_test.cc)
    target_link_libraries(aktualizr_test_rvi_client aktualizr_static_lib ${TEST_LIBS})
    add_test(NAME test_rvi_client COMMAND aktualizr_test_rvi_client WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    add_dependencies(check aktualizr_test_rvi_client)

    add_executable(aktualizr_test_swm tests/swm_test.cc)
    target_compile_options(aktualizr_test_swm PUBLIC ${LIBDBUS_CFLAGS})
    target_link_libraries(aktualizr_test_swm aktualizr_static_lib ${TEST_LIBS} ${LIBDBUS_LIBRARIES})
    add_test(NAME test_swm COMMAND dbus-run-session --config-file ${PROJECT_SOURCE_DIR}/tests/session.conf bash -c "${CMAKE_BINARY_DIR}/aktualizr_test_swm ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    add_dependencies(check aktualizr_test_swm)
endif(BUILD_GENIVI)

if(BUILD_WITH_CODE_COVERAGE)
    add_dependencies(coverage aktualizr)
    add_dependencies(coverage aktualizr_test_config)
    add_dependencies(coverage aktualizr_test_events)
    add_dependencies(coverage aktualizr_test_commands)
    if(BUILD_GENIVI)
        add_dependencies(coverage aktualizr_test_dbusgateway)
    endif(BUILD_GENIVI)
    add_dependencies(coverage aktualizr_test_http_aktualizr)
    add_dependencies(coverage aktualizr_test_http_client)
    add_dependencies(coverage aktualizr_test_socket_gateway)
    add_dependencies(coverage aktualizr_test_rvi_client)
    add_dependencies(coverage aktualizr_test_swm)
endif(BUILD_WITH_CODE_COVERAGE)

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
# vim: set tabstop=4 shiftwidth=4 expandtab:
