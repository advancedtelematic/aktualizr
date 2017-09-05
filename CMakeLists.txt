cmake_minimum_required (VERSION 2.6)

project(aktualizr)

option(WARNING_AS_ERROR "Treat warnings as errors" ON)
option(PEDANTIC_WARNINGS "Compile with pedantic warnings" OFF)
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)
option(BUILD_GENIVI "Set to ON to compile with SWM and RVI gateway support" OFF)
option(BUILD_OSTREE "Set to ON to compile with ostree and Uptane support" OFF)
option(INSTALL_LIB "Set to ON to install library and headers" OFF)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "Aktualizr does not support building in the source tree. Please remove CMakeCache.txt and the CMakeFiles/ directory, then create a subdirectory to build in: mkdir build; cd build; cmake ..")
endif()

if(AKTUALIZR_VERSION)
    add_definitions(-DAKTUALIZR_VERSION="${AKTUALIZR_VERSION}")
else(AKTUALIZR_VERSION)
    add_definitions(-DAKTUALIZR_VERSION="dev")
endif(AKTUALIZR_VERSION)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules)

# find all required libraries
find_package(PkgConfig REQUIRED)
find_package(Boost COMPONENTS system filesystem thread program_options log log_setup regex chrono date_time random REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(LibArchive REQUIRED)
find_package(LibP11 REQUIRED)
pkg_search_module(SODIUM libsodium REQUIRED)

if(BUILD_OSTREE)
    find_package(OSTree REQUIRED)
    add_definitions(-DBUILD_OSTREE)
endif(BUILD_OSTREE)

if(BUILD_GENIVI)
    pkg_search_module(LIBDBUS REQUIRED dbus-1)
    add_definitions(-DWITH_GENIVI)
endif(BUILD_GENIVI)

# set symbols used when compiling
add_definitions(-DBOOST_LOG_DYN_LINK)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No CMAKE_BUILD_TYPE specified, defaulting to Debug")
    set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")


################ QA RULES
add_custom_target(qa)
add_custom_target(check-format)

add_custom_target(format)
add_dependencies(qa format)

# clang-check and clang-format
# The .clang-format file requires clang-format-3.8
find_program(CLANG_FORMAT NAMES clang-format-3.8 clang-format)
find_program(CLANG_CHECK NAMES clang-check-3.8 clang-check)

############### BUILD RULES
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/third_party/jsoncpp)
include_directories(${PROJECT_SOURCE_DIR}/third_party/picojson)
include_directories(${PROJECT_SOURCE_DIR}/third_party/rvi_lib/include)
include_directories(${LIBOSTREE_INCLUDE_DIRS})
include_directories(${LIBDBUS_INCLUDE_DIRS})

if(BUILD_GENIVI)
    set(CMAKE_CXX_STANDARD 11)
    add_subdirectory("third_party/rvi_lib")
endif(BUILD_GENIVI)

# Setup warnings. Do this after rvi_lib is added so that it isn't affected.
if (CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-fstack-protector-all)
    # Enable maximum set of warnings. -Wno-sign-compare is required because of
    # problems in gtest. -Wswitch-default and -Wconversion would be nice as
    # well, but they also cause problems in gtest.
    add_definitions(-Wall -Wextra -Wformat-security -Wfloat-equal -Wcast-qual -Wlogical-op -Wno-sign-compare)

    if (CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "4.9" OR MAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.9")
        add_definitions(-Wshadow)
    endif ()

    if(WARNING_AS_ERROR)
        add_definitions(-Werror)
    endif()

    if (PEDANTIC_WARNINGS)
        add_definitions(-Wpedantic -Wswitch-default -Wsign-compare -Wconversion)
    endif (PEDANTIC_WARNINGS)
endif()

add_subdirectory("src")
add_subdirectory("src/cert_provider")

include(CTest)
add_subdirectory("tests" EXCLUDE_FROM_ALL)

add_subdirectory("docs" EXCLUDE_FROM_ALL)

# Generate ctags
set_source_files_properties(tags PROPERTIES GENERATED true)
add_custom_target(tags
    COMMAND ctags -R --c++-kinds=+p --fields=+iaS --extra=+q src
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# vim: set tabstop=4 shiftwidth=4 expandtab:
