cmake_minimum_required(VERSION 2.8.8)
project(sota-client)

# When building inside Yocto, provide the option to disable -Werror, otherwise
# new compiler warnings will break the build.  Don't set to OFF in development!
option(WARNING_AS_ERROR "Treat warnings as errors" ON)

# The tests can optionally test authentication against Auth+. To do this they
# require a client id & secret. Specify credentials here to enable these tests
set(OAUTH2_TEST_CLIENT_ID "" CACHE STRING "Client ID for testing Auth+ authentication")
set(OAUTH2_TEST_CLIENT_SECRET "" CACHE STRING "Client Secret for testing Auth+ authentication")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake-modules")

# clang-check and clang-format
find_program(CLANG_FORMAT NAMES clang-format clang-format-3.7 clang-format-3.6)
find_program(CLANG_CHECK NAMES clang-check clang-check-3.7 clang-check-3.6 clang-check-3.5 clang-check-3.4)

# pandoc (optional) for test rendering of README.adoc
find_program(PANDOC NAMES pandoc)

# Boost
find_package(Boost REQUIRED program_options filesystem system log)
include_directories(${Boost_INCLUDE_DIRS})
add_definitions(-DBOOST_LOG_DYN_LINK=1)

# Curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Glib
find_package(GLIB2 REQUIRED)
include_directories(${GLIB2_INCLUDE_DIRS})

find_package(Threads REQUIRED)
# strace
find_program(STRACE NAMES strace)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

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

set (CMAKE_CXX_STANDARD 11)

# QA virtual target
add_custom_target(qa)

enable_testing()

# Cause garage push to be in build rather than build/tools
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_subdirectory(tools)

# Generate config.h from config.h.in
#configure_file(config.h.in config.h)
#include_directories(${CMAKE_CURRENT_BINARY_DIR})


message("Build type is ${CMAKE_BUILD_TYPE}")

# Generate ctags
set_source_files_properties(tags PROPERTIES GENERATED true)
add_custom_target(tags
    COMMAND ctags -R --c++-kinds=+p --fields=+iaS --extra=+q tools
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})


if(PANDOC)
    # Test render README.adoc
    add_custom_command(
        OUTPUT README.html
        COMMAND ${PANDOC} -o README.html --self-contained -t html5 ${CMAKE_SOURCE_DIR}/README.adoc
        DEPENDS README.adoc
        VERBATIM)
    add_custom_target(readme DEPENDS README.html)
    add_dependencies(qa readme)
endif() # PANDOC

# vim: set tabstop=4 shiftwidth=4 expandtab:
