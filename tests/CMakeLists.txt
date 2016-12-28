###############################################################################

# Setup quality and coverage features

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
                    tests/events_test.cc)


if(CLANG_FORMAT)
add_custom_target(format-tools
    COMMAND ${CLANG_FORMAT} -i -style Google ${SOURCES} ${HEADERS} ${ALL_TEST_SRCS}
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
  DEPENDS sota_client)

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
               ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})



unset(TEST_SRCS)
set(TEST_SRCS src/logger.cc
              src/logger.h
              src/config.cc
              tests/config_test.cc)
add_executable(sota_client_test_config ${TEST_SRCS})

# set the libraries for the current target
target_link_libraries(sota_client_test_config ${TEST_LIBS})


unset(TEST_SRCS)
set(TEST_SRCS   src/types.cc
                src/events.cc
                tests/events_test.cc)
add_executable(sota_client_test_events ${TEST_SRCS})

# set the libraries for the current target
target_link_libraries(sota_client_test_events ${TEST_LIBS})


###############################################################################

# add the test target to coverage analysis
if(BUILD_WITH_CODE_COVERAGE)
target_link_libraries(sota_client_test_config gcov)
add_dependencies(coverage sota_client_test_config)
target_link_libraries(sota_client_test_events gcov)
add_dependencies(coverage sota_client_test_events)

endif(BUILD_WITH_CODE_COVERAGE)


# declares a test using the test target
add_test(NAME test_config COMMAND sota_client_test_config WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
file(WRITE Testing/config.toml "")
add_test(NAME test_events COMMAND sota_client_test_events WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

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