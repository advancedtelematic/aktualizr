

IF( NOT GTEST_ROOT )
    set(GTEST_ROOT /usr/src/gtest)
ENDIF()

IF( NOT GMOCK_ROOT )
    set(GMOCK_ROOT /usr/src/gmock)
ENDIF()

# build google test as static library
set(BUILD_SHARED_LIBS OFF)
set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 1)

add_subdirectory(${GMOCK_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gmock EXCLUDE_FROM_ALL)
#add_subdirectory(${GTEST_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gtest EXCLUDE_FROM_ALL)

set(BUILD_SHARED_LIBS ON)
set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules)

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


# Setup linting

# clang-check and clang-format
find_program(CLANG_FORMAT NAMES clang-format clang-format-3.8 clang-format-3.7 clang-format-3.6 clang-format-3.5)
find_program(CLANG_CHECK NAMES clang-check clang-check-3.8 clang-check-3.7 clang-check-3.6 clang-check-3.5 clang-check-3.4)

# Setup coverage

if(BUILD_WITH_CODE_COVERAGE)
    include(CodeCoverage)
    setup_target_for_coverage(coverage ctest coverage)
    add_definitions(-g -O0 -fprofile-arcs -ftest-coverage)
    target_link_libraries(sota_client gcov)
    add_dependencies(coverage sota_client)
endif()

add_custom_target(qa)

# Export compile_commands.json for clang-check
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(ALL_TEST_SRCS   tests/config_test.cc
                    tests/events_test.cc
                    tests/commands_test.cc
                    test/httpsotaclient_test.cc
                    test/httpclient_test.cc)

if(CLANG_FORMAT)
add_custom_target(format-tools
    COMMAND ${CLANG_FORMAT} -i -style file ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cc
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-format"
    VERBATIM)
add_dependencies(qa format-tools)
else()
    message(WARNING "clang-format not found, skipping")
endif()

add_custom_target(check-tools
    COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} ${SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-check"
    VERBATIM)
add_dependencies(qa check-tools sota_client)

###############################################################################

# Workaround a cmake bug where tests are not dependent on their executables
# http://stackoverflow.com/questions/733475/cmake-ctest-make-test-doesnt-build-tests#736838
add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=1 ${CMAKE_CTEST_COMMAND}
  DEPENDS sota_client_test_config sota_client_test_commands sota_client_test_events)

add_dependencies(qa check)

###############################################################################

#Setup CMake to run tests

# using boost test requires using static linking of libraries
set(Boost_USE_STATIC_LIBS ON)
# let cmake find the boost test library
find_package(Boost COMPONENTS unit_test_framework REQUIRED)

#enable test features of cmake
enable_testing()

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src)


# use the same libiraries as the normal target but add boost test
set (TEST_LIBS ${LINK_LIBS}
               ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} gtest gmock)


# Config Test
set(TEST_CONFIG_SRC src/logger.cc
              src/logger.h
              src/config.cc
              tests/config_test.cc)
add_executable(sota_client_test_config ${TEST_CONFIG_SRC})
target_link_libraries(sota_client_test_config ${TEST_LIBS})


set(TEST_SRCS   third_party/jsoncpp/jsoncpp.cpp
                src/types.cc
                src/events.cc
                tests/events_test.cc)
add_executable(sota_client_test_events ${TEST_SRCS})

# set the libraries for the current target
target_link_libraries(sota_client_test_events ${TEST_LIBS})


unset(TEST_SRCS)
set(TEST_SRCS   third_party/jsoncpp/jsoncpp.cpp
                src/types.cc
                src/commands.cc
                tests/commands_test.cc)
add_executable(sota_client_test_commands ${TEST_SRCS})

# set the libraries for the current target
target_link_libraries(sota_client_test_commands ${TEST_LIBS})

if(BUILD_WITH_DBUS_GATEWAY)
    unset(TEST_SRCS)
    set(TEST_SRCS   third_party/jsoncpp/jsoncpp.cpp
                    src/types.cc
                    src/commands.cc
                    src/events.cc
                    src/logger.cc
                    tests/dbusgateway_test.cc)
    add_executable(sota_client_test_dbusgateway ${TEST_SRCS} ${DBUS_SOURCES})
    target_compile_options(sota_client_test_dbusgateway PUBLIC ${LIBDBUS_CFLAGS})

    # set the libraries for the current target
    target_link_libraries(sota_client_test_dbusgateway ${TEST_LIBS} ${DBUS_LIBS})
endif(BUILD_WITH_DBUS_GATEWAY)

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
add_executable(sota_client_test_http_sota_client ${TEST_HTTP_SOTA_SRC})
target_link_libraries(sota_client_test_http_sota_client ${TEST_LIBS})
add_test(NAME test_http_sota_client COMMAND sota_client_test_http_sota_client WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


set(TEST_HTTP_SRC src/logger.cc
              src/logger.cc
              third_party/jsoncpp/jsoncpp.cpp
              src/config.cc
              src/httpclient.cc
              tests/httpclient_test.cc)
add_executable(sota_client_test_http_client ${TEST_HTTP_SRC})
target_link_libraries(sota_client_test_http_client ${TEST_LIBS})

add_test(NAME test_http_client COMMAND sota_client_test_http_client ${PROJECT_SOURCE_DIR}/tests/fake_http_server/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

set(TEST_SOCKET_SRC src/logger.cc
              src/logger.cc
              third_party/jsoncpp/jsoncpp.cpp
              src/config.cc
              src/commands.cc
              src/events.cc
              src/types.cc
              src/socketgateway.cc
              tests/socketgateway_test.cc)
add_executable(sota_client_test_socket_gateway ${TEST_SOCKET_SRC})
target_link_libraries(sota_client_test_socket_gateway ${LINK_LIBS} gtest)

add_test(NAME test_socket_gateway COMMAND sota_client_test_socket_gateway ${PROJECT_SOURCE_DIR}/tests/fake_unix_socket/ WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})



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
link_directories(${RVI_LINK_DIR})
target_link_libraries(aktualizr_test_rvi_client ${LINK_LIBS} gtest)
add_dependencies(aktualizr_test_rvi_client rvi_lib)
add_test(NAME test_rvi_client COMMAND aktualizr_test_rvi_client WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})


set(TEST_SWM_SRC third_party/jsoncpp/jsoncpp.cpp
                    src/types.cc
                    src/commands.cc
                    src/events.cc
                    src/logger.cc
                    tests/swm_test.cc)
add_executable(aktualizr_test_swm ${TEST_SWM_SRC} ${DBUS_SOURCES})
target_compile_options(aktualizr_test_swm PUBLIC ${LIBDBUS_CFLAGS})
target_link_libraries(aktualizr_test_swm ${LINK_LIBS} ${DBUS_LIBS} gtest)
add_test(NAME test_swm COMMAND dbus-run-session bash -c "${CMAKE_BINARY_DIR}/target/aktualizr_test_swm ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})




###############################################################################

# add the test target to coverage analysis
if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(sota_client_test_config gcov)
    add_dependencies(coverage sota_client_test_config)
    target_link_libraries(sota_client_test_events gcov)
    add_dependencies(coverage sota_client_test_events)
    target_link_libraries(sota_client_test_commands gcov)
    add_dependencies(coverage sota_client_test_commands)
    if(BUILD_WITH_DBUS_GATEWAY)
        target_link_libraries(sota_client_test_dbusgateway gcov)
        add_dependencies(coverage sota_client_test_dbusgateway)
    endif(BUILD_WITH_DBUS_GATEWAY)
    target_link_libraries(sota_client_test_http_sota_client gcov)
    add_dependencies(coverage sota_client_test_http_sota_client)
    target_link_libraries(sota_client_test_http_client gcov)
    add_dependencies(coverage sota_client_test_http_client)
    target_link_libraries(sota_client_test_socket_gateway gcov)
    add_dependencies(coverage sota_client_test_socket_gateway)
    target_link_libraries(aktualizr_test_rvi_client gcov)
    add_dependencies(coverage aktualizr_test_rvi_client)
    target_link_libraries(aktualizr_test_swm gcov)
    add_dependencies(coverage aktualizr_test_swm)
endif(BUILD_WITH_CODE_COVERAGE)


    # declares a test using the test target
    add_test(NAME test_config COMMAND sota_client_test_config WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    file(WRITE Testing/config.toml "")
    add_test(NAME test_events COMMAND sota_client_test_events WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    add_test(NAME test_commands COMMAND sota_client_test_commands WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    if(BUILD_WITH_DBUS_GATEWAY)
        add_test(NAME test_dbusgateway COMMAND dbus-run-session bash -c "${CMAKE_BINARY_DIR}/target/sota_client_test_dbusgateway ${PROJECT_SOURCE_DIR}/tests/fake_dbus_tools/" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    endif(BUILD_WITH_DBUS_GATEWAY)

###############################################################################
# The test feature of cmake checks the return value when the program
# exits. If the return value is zero, the testcase passes.

# test running the executable with command line option --help
add_test(NAME feat1_test--help
         COMMAND sota_client --help)
# test running the executable with command line option -h
add_test(NAME feat1_test-h
         COMMAND sota_client -h)
# test running the executable with command line option --something
add_test(NAME feat1_test--something
         COMMAND sota_client --something -c ${PROJECT_SOURCE_DIR}/config/config.toml.example)
# test running the executable with command line option -sth
add_test(NAME feat1_test-sth
         COMMAND sota_client -sth -c ${PROJECT_SOURCE_DIR}/config/config.toml.example)


# calle the executable without any options
add_test(NAME test_plain
         COMMAND sota_client)

# check invalid logging level

add_test(NAME test_log_invalid
         COMMAND sota_client --loglevel=5)


# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass
set_tests_properties(feat1_test--something
                     feat1_test-sth
                     test_plain
                     test_log_invalid
                     PROPERTIES WILL_FAIL TRUE)
