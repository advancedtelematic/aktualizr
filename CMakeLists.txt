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

# find all required libraries
find_package(Boost COMPONENTS system thread program_options log log_setup regex REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(LIBCURL REQUIRED libcurl)

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

# set local include directory(ies)
set(INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/inc/)

# set variable for the source directory
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/)

# tell cmake to include directory(ies)
include_directories(${INC_DIR}
                    ${INC_DIR}srv/)


# set source files excluded main for using the list for the test target
set(SOURCES ${SRC_DIR}logger.cpp
            ${INC_DIR}logger.hpp
            ${SRC_DIR}ymlcfg.cpp
            ${INC_DIR}ymlcfg.hpp)
            
# add subfolders
add_subdirectory("${SRC_DIR}/srv")
add_subdirectory("${INC_DIR}/srv")

# introduce a new variable containing source including main
set(MAIN_SRCS ${SOURCES}
              ${SRC_DIR}main.cpp)
            
# set output folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY target/)

# set the name of the executable
add_executable(sota_client ${MAIN_SRCS})

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
    COMMAND ${CLANG_FORMAT} -i -style Google ${MAIN_SRCS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-format"
    VERBATIM)
add_dependencies(qa format-tools)
else()
    message(WARNING "clang-format not found, skipping")
endif()

add_custom_target(check-tools
    COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} ${MAIN_SRCS}
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
include(test_src/CMakeLists.txt)
