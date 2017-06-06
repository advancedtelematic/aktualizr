cmake_minimum_required (VERSION 2.6)

project(aktualizr)

option(WARNING_AS_ERROR "Treat warnings as errors" ON)
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)
option(BUILD_GENIVI "Set to ON to compile with SWM and RVI gateway support" OFF)
option(BUILD_OSTREE "Set to ON to compile with ostree and Uptane support" OFF)
option(BUILD_TESTS "Build tests (requires gtest/gmock)" ON)
option(INSTALL_LIB "Set to ON to install library and headers" OFF)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "Aktualizr does not support building in the source tree. Please remove CMakeCache.txt and the CMakeFiles/ directory, then create a subdirectory to build in: mkdir build; cd build; cmake ..")
endif()

# clang-check and clang-format
# The .clang-format file requires clang-format-3.8
find_program(CLANG_FORMAT NAMES clang-format clang-format-3.8)
find_program(CLANG_CHECK NAMES clang-check clang-check-3.8 clang-check-3.7 clang-check-3.6 clang-check-3.5 clang-check-3.4)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No CMAKE_BUILD_TYPE specified, defaulting to Debug")
    set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

if(BUILD_GENIVI)
    add_subdirectory("third_party/rvi_lib")
endif(BUILD_GENIVI)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_target(qa)
#add_subdirectory("src")
# FIXME temporary use include because of bugs with coverage
include("src/CMakeLists.txt")

if(BUILD_TESTS)
    include("tests/CMakeLists.txt")
endif(BUILD_TESTS)

if(BUILD_GENIVI)
    install(FILES dbus/org.genivi.SotaClient.conf DESTINATION /etc/dbus-1/system.d)
endif(BUILD_GENIVI)

# Generate ctags
set_source_files_properties(tags PROPERTIES GENERATED true)
add_custom_target(tags
    COMMAND ctags -R --c++-kinds=+p --fields=+iaS --extra=+q src
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# vim: set tabstop=4 shiftwidth=4 expandtab:
