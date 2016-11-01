cmake_minimum_required(VERSION 2.8.8)
project(sota-client)

# clang-check and clang-format
find_program(CLANG_FORMAT NAMES clang-format clang-format-3.7 clang-format-3.6 clang-format-3.5 clang-format-3.4)
find_program(CLANG_CHECK NAMES clang-check clang-check-3.7 clang-check-3.6 clang-check-3.5 clang-check-3.4)

# Boost
find_package(Boost REQUIRED program_options filesystem system)
include_directories(${Boost_INCLUDE_DIRS})

# Curl
find_package(CURL)
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

find_package(OpenSSL REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

if (CMAKE_COMPILER_IS_GNUCXX)
        add_definitions(-fstack-protector-all)
        # Enable maximum of Warnings :
        add_definitions(-Wall -Wextra -Wswitch-default -Wswitch-enum -Winit-self -Wformat-security -Wfloat-equal -Wcast-qual -Wconversion -Wlogical-op -Winline)
        if (CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "4.9" OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.9")
            add_definitions (-Wfloat-conversion)
            add_definitions (-Wshadow)
        endif ()
endif()

# QA virtual target
add_custom_target(qa)

enable_testing()

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

