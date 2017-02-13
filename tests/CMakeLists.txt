

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

# Setup coverage
if(BUILD_WITH_CODE_COVERAGE)
    include(CodeCoverage)
    setup_target_for_coverage(coverage ctest coverage)
    add_definitions(-g -O0 -fprofile-arcs -ftest-coverage)
    target_link_libraries(aktualizr gcov)
    add_dependencies(coverage aktualizr)
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
add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=1 ${CMAKE_CTEST_COMMAND}
  DEPENDS aktualizr_test_config aktualizr_test_commands aktualizr_test_events)

add_dependencies(qa check)

###############################################################################

#Setup CMake to run tests
# let cmake find the boost test library
find_package(Boost COMPONENTS system thread program_options log log_setup regex unit_test_framework REQUIRED)

#enable test features of cmake
enable_testing()

include_directories(${PROJECT_SOURCE_DIR}/src)

# use the same libiraries as the normal target but add boost test
set (TEST_LIBS ${Boost_LIBRARIES} ${CURL_LIBRARIES}
               ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} gtest gmock)

# Config Test
set(TEST_CONFIG_SRC src/logger.cc
              src/logger.h
              src/config.cc
              tests/config_test.cc)
add_executable(aktualizr_test_config ${TEST_CONFIG_SRC})
target_link_libraries(aktualizr_test_config ${TEST_LIBS})


set(TEST_SRCS   third_party/jsoncpp/jsoncpp.cpp
                src/types.cc
                src/events.cc
                tests/events_test.cc)
add_executable(aktualizr_test_events ${TEST_SRCS})
target_link_libraries(aktualizr_test_events ${TEST_LIBS})


unset(TEST_SRCS)
set(TEST_SRCS   third_party/jsoncpp/jsoncpp.cpp
                src/types.cc
                src/commands.cc
                tests/commands_test.cc)
add_executable(aktualizr_test_commands ${TEST_SRCS})
target_link_libraries(aktualizr_test_commands ${TEST_LIBS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++0x")

######## HttpSotaClientTest
set(TEST_HTTP_SOTA_SRC src/logger.cc
              src/logger.cc
              third_party/jsoncpp/jsoncpp.cpp
              src/config.cc
              src/httpclient.cc
              src/sotahttpclient.cc
              src/types.cc
              src/commands.cc
              src/events.cc
              tests/httpsotaclient_test.cc)
add_executable(aktualizr_test_http_aktualizr ${TEST_HTTP_SOTA_SRC})
target_link_libraries(aktualizr_test_http_aktualizr ${TEST_LIBS})
add_test(NAME test_http_aktualizr COMMAND aktualizr_test_http_aktualizr WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


set(TEST_HTTP_SRC src/logger.cc
              src/logger.cc
              third_party/jsoncpp/jsoncpp.cpp
              src/config.cc
              src/httpclient.cc
              tests/httpclient_test.cc)
add_executable(aktualizr_test_http_client ${TEST_HTTP_SRC})
target_link_libraries(aktualizr_test_http_client ${TEST_LIBS})

add_test(NAME test_http_client COMMAND aktualizr_test_http_client ${PROJECT_SOURCE_DIR}/tests/fake_http_server/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

set(TEST_SOCKET_SRC src/logger.cc
              src/logger.cc
              third_party/jsoncpp/jsoncpp.cpp
              src/config.cc
              src/commands.cc
              src/events.cc
              src/types.cc
              src/socketgateway.cc
              tests/socketgateway_test.cc)
add_executable(aktualizr_test_socket_gateway ${TEST_SOCKET_SRC})
target_link_libraries(aktualizr_test_socket_gateway ${TEST_LIBS} gtest)

add_test(NAME test_socket_gateway COMMAND aktualizr_test_socket_gateway ${PROJECT_SOURCE_DIR}/tests/fake_unix_socket/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

if(BUILD_GENIVI)
    unset(TEST_SRCS)
    set(TEST_SRCS third_party/jsoncpp/jsoncpp.cpp
                  src/types.cc
                  src/commands.cc
                  src/events.cc
                  src/logger.cc
                  src/dbusgateway/dbusgateway.cc
                  src/dbusgateway/swlm.cc
                  tests/dbusgateway_test.cc)
    add_executable(aktualizr_test_dbusgateway ${TEST_SRCS})
    target_compile_options(aktualizr_test_dbusgateway PUBLIC ${LIBDBUS_CFLAGS})
    target_link_libraries(aktualizr_test_dbusgateway ${TEST_LIBS} ${LIBDBUS_LIBRARIES})

    set(TEST_RVI_SRC src/logger.cc
                     src/logger.cc
                     third_party/jsoncpp/jsoncpp.cpp
                     src/config.cc
                     src/commands.cc
                     src/events.cc
                     src/types.cc
                     src/sotarviclient.cc
                     src/logger.cc
                     tests/rvisotaclient_test.cc)
    add_executable(aktualizr_test_rvi_client ${TEST_RVI_SRC})
    target_link_libraries(aktualizr_test_rvi_client rvi ${TEST_LIBS} gtest)
    add_test(NAME test_rvi_client COMMAND aktualizr_test_rvi_client WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    set(TEST_SWM_SRC third_party/jsoncpp/jsoncpp.cpp
                     src/types.cc
                     src/commands.cc
                     src/events.cc
                     src/logger.cc
                     src/dbusgateway/dbusgateway.cc
                     src/dbusgateway/swlm.cc
                     tests/swm_test.cc)
    add_executable(aktualizr_test_swm ${TEST_SWM_SRC} ${DBUS_SOURCES})
    target_compile_options(aktualizr_test_swm PUBLIC ${LIBDBUS_CFLAGS})
    target_link_libraries(aktualizr_test_swm ${TEST_LIBS} ${LIBDBUS_LIBRARIES} gtest)
    add_test(NAME test_swm COMMAND dbus-run-session --config-file ${PROJECT_SOURCE_DIR}/tests/session.conf bash -c "${CMAKE_BINARY_DIR}/aktualizr_test_swm ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif(BUILD_GENIVI)



###############################################################################
# add the test target to coverage analysis
if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr_test_config gcov)
    add_dependencies(coverage aktualizr_test_config)
    target_link_libraries(aktualizr_test_events gcov)
    add_dependencies(coverage aktualizr_test_events)
    target_link_libraries(aktualizr_test_commands gcov)
    add_dependencies(coverage aktualizr_test_commands)
    if(BUILD_GENIVI)
        target_link_libraries(aktualizr_test_dbusgateway gcov)
        add_dependencies(coverage aktualizr_test_dbusgateway)
    endif(BUILD_GENIVI)
    target_link_libraries(aktualizr_test_http_aktualizr gcov)
    add_dependencies(coverage aktualizr_test_http_aktualizr)
    target_link_libraries(aktualizr_test_http_client gcov)
    add_dependencies(coverage aktualizr_test_http_client)
    target_link_libraries(aktualizr_test_socket_gateway gcov)
    add_dependencies(coverage aktualizr_test_socket_gateway)
    target_link_libraries(aktualizr_test_rvi_client gcov)
    add_dependencies(coverage aktualizr_test_rvi_client)
    target_link_libraries(aktualizr_test_swm gcov)
    add_dependencies(coverage aktualizr_test_swm)
endif(BUILD_WITH_CODE_COVERAGE)


# declares a test using the test target
add_test(NAME test_config COMMAND aktualizr_test_config WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
file(WRITE Testing/config.toml "")
add_test(NAME test_events COMMAND aktualizr_test_events WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
add_test(NAME test_commands COMMAND aktualizr_test_commands WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

if(BUILD_GENIVI)
    add_test(NAME test_dbusgateway COMMAND dbus-run-session --config-file ${PROJECT_SOURCE_DIR}/tests/session.conf bash -c "${CMAKE_BINARY_DIR}/aktualizr_test_dbusgateway ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
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
# vim: set tabstop=4 shiftwidth=4 expandtab:
