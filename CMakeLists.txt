##
# cmake Makefile for the sota_client_cpp
# 
# Copyright (c) 2016 Advanced Telematic Systems GmbH
# 
# Author: Moritz Klinger - mklinger@embeddeers.com 
#

# define the minimum cmake version that is required
cmake_minimum_required (VERSION 2.6)

# define a project name
PROJECT(sota_client_cpp)

# optionally disable treating warnings as errors
option(WARNING_AS_ERROR "Treat warnings as errors" ON)

# enable code coverage
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)

# set build type to Debug or Release here
set(CMAKE_BUILD_TYPE Debug)

# set symbols used when compiling
add_definitions(-DBOOST_LOG_DYN_LINK=1)

# configure libraries
set(LINK_LIBS libpthread.so
              libboost_system.so
              libboost_thread.so
              libboost_log_setup.so 
              libboost_log.so
              libboost_program_options.so
              libyaml-cpp.so)

# set local include directory(ies)
set(INC_DIR inc/)

# set variable for the source directory
set(SRC_DIR src/)

# tell cmake to include directory(ies)
include_directories(${INC_DIR})

# set source files
set(SOURCES ${SRC_DIR}main.cpp
            ${SRC_DIR}logger.cpp
            ${INC_DIR}logger.hpp
            ${SRC_DIR}ymlcfg.cpp
            ${INC_DIR}ymlcfg.hpp)
            
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
    COMMAND ${CLANG_FORMAT} -i -style Google ${SOURCES}
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

# enable cmakes test functionality
enable_testing()

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
         COMMAND sota_client --something)
# test running the executable with command line option -sth
add_test(NAME feat1_test-sth
         COMMAND sota_client -sth)
# test running the executable without command line options
add_test(NAME feat1_test-no_opt
         COMMAND sota_client)
         
#test calling the executable with a config file         
add_test(NAME feat2_test_config_file
         COMMAND sota_client -c ../config/config.yml.example)
         
# calle the executable without any options
add_test(NAME test_plain
         COMMAND sota_client) 
         
         
# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass         
set_tests_properties(feat1_test--something
                     feat1_test-sth 
                     PROPERTIES WILL_FAIL TRUE)
                     
# set the config file test to match the content of the config file as the config file
# contains a loglevel 1 (degub) where the content of the config file is logged to stderr 
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "thisisaclientid")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "thisisaclientsecret")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "https://url.com")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "Server: https://url.com")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "Loglevel: 1")

# Workaround a cmake bug where tests are not dependent on their executables
# http://stackoverflow.com/questions/733475/cmake-ctest-make-test-doesnt-build-tests#736838
add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=1 ${CMAKE_CTEST_COMMAND}
  DEPENDS sota_client)

add_dependencies(qa check) 
