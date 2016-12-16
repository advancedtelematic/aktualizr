# set symbols used when compiling
add_definitions(-DBOOST_LOG_DYN_LINK=1)

# find all required libraries
find_package(Boost COMPONENTS system thread program_options log log_setup regex REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(LIBCURL REQUIRED libcurl)
pkg_search_module(YAML_CPP REQUIRED yaml-cpp)
# configure libraries
set(LINK_LIBS libpthread.so
              ${Boost_SYSTEM_LIBRARY}
              ${Boost_THREAD_LIBRARY}
              ${Boost_LOG_LIBRARIES}
              ${Boost_LOG_SETUP_LIBRARIES}
              ${Boost_PROGRAM_OPTIONS_LIBRARY}
              ${Boost_REGEX_LIBRARY}
              ${YAML_CPP_LIBRARIES}
              ${LIBCURL_LIBRARIES})


# set source files excluded main for using the list for the test target
set(SOURCES src/logger.cpp
            src/ymlcfg.cpp
            src/oauthtoken.cpp
            src/servercon.cpp
            src/main.cpp)

set(HEADERS src/logger.hpp
            src/ymlcfg.hpp
            src/oauthtoken.hpp
            src/servercon.hpp)


# set output folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY target/)

# set the name of the executable
add_executable(sota_client ${SOURCES})

# define libraries for the target
target_link_libraries(sota_client ${LINK_LIBS})

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

if(CLANG_FORMAT)
add_custom_target(format-tools
    COMMAND ${CLANG_FORMAT} -i -style Google ${SOURCES} ${HEADERS}
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


# define the sources for the test
set(TEST_SRCS  ${SOURCES}
               src/servercon_test.cpp)

# set the name of the executable
add_executable(sota_client_test ${TEST_SRCS})

# indicates the include paths
target_include_directories(sota_client_test PRIVATE ${TEST_INC_DIRS})

# use the same libiraries as the normal target but add boost test
set (TEST_LIBS ${LINK_LIBS}
               ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

###############################################################################

# set the libraries for the current target
target_link_libraries(sota_client_test ${TEST_LIBS})


unset(TEST_SRCS)
set(TEST_SRCS src/oauthtoken.hpp
              src/logger.cpp
              src/logger.hpp
              src/servercon.cpp
              src/oauthtoken_test.cpp)

              # set the name of the executable
add_executable(sota_client_test1 ${TEST_SRCS})


# set the libraries for the current target
target_link_libraries(sota_client_test1 ${TEST_LIBS})

###############################################################################

# add the test target to coverage analysis
if(BUILD_WITH_CODE_COVERAGE)
target_link_libraries(sota_client_test gcov)
add_dependencies(coverage sota_client_test)
target_link_libraries(sota_client_test1 gcov)
add_dependencies(coverage sota_client_test1)
endif(BUILD_WITH_CODE_COVERAGE)

# declares a test using the test target
add_test(NAME test_servercon COMMAND sota_client_test)


# declares a test using the test target
add_test(NAME test_oauthtoken COMMAND sota_client_test1)

###############################################################################
# The test feature of cmake checks the return value when the program
# exits. If the return value is zero, the testcase passes.

# test running the executable with command line option --help
add_test(NAME feat1_test--help
         COMMAND sota_client --help -c ../config/config.yml.example)
# test running the executable with command line option -h
add_test(NAME feat1_test-h
         COMMAND sota_client -h -c ../config/config.yml.example)
# test running the executable with command line option --something
add_test(NAME feat1_test--something
         COMMAND sota_client --something -c ../config/config.yml.example)
# test running the executable with command line option -sth
add_test(NAME feat1_test-sth
         COMMAND sota_client -sth -c ../config/config.yml.example)

#test calling the executable with a config file
add_test(NAME feat2_test_config_file
         COMMAND sota_client -c ../config/config.yml.example)

# calle the executable without any options
add_test(NAME test_plain
         COMMAND sota_client)

# check possible logging levels
add_test(NAME test_log_trace
         COMMAND sota_client --loglevel=0 -c ../config/config.yml.example)

add_test(NAME test_log_debug
         COMMAND sota_client --loglevel=1 -c ../config/config.yml.example)

add_test(NAME test_log_warning
         COMMAND sota_client --loglevel=2 -c ../config/config.yml.example)

add_test(NAME test_log_info
         COMMAND sota_client --loglevel=3 -c ../config/config.yml.example)

add_test(NAME test_log_error
         COMMAND sota_client --loglevel=4 -c ../config/config.yml.example)

add_test(NAME test_log_invalid
         COMMAND sota_client --loglevel=5 -c ../config/config.yml.example)


# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass
set_tests_properties(feat1_test--something
                     feat1_test-sth
                     test_plain
                     PROPERTIES WILL_FAIL TRUE)

# set the config file test to match the content of the config file as the config file
# contains a loglevel 1 (degub) where the content of the config file is logged to stderr
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "thisisaclientid")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "thisisaclientsecret")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "https://url.com")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "Server: https://url.com")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "Loglevel: 1")
