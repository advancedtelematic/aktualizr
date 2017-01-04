cmake_minimum_required (VERSION 2.6)

project(sota_client_cpp)

option(WARNING_AS_ERROR "Treat warnings as errors" ON)
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#add_subdirectory("src")
# FIXME temporary use include because of bugs with coverage
include("src/CMakeLists.txt")
include("tests/CMakeLists.txt")
