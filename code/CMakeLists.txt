##
# cmake Makefile for the sota_client_cpp
# 
# In order to build the program, go to the
# code folder and execute :
# $ cmake CMakeLists.txt
# $ make
#
# (c) ?? 2016
# 
# Author: Moritz Klinger - mklinger@embeddeers.com 
#

# define the minimum cmake version that is required
cmake_minimum_required (VERSION 2.6)

# define a project name
PROJECT(sota_client_cpp)

# set build type to Debug or Release here
set(CMAKE_BUILD_TYPE Release)

# set local include directories
include_directories(../code/inc)

# configure libraries
set(LINK_LIBS libboost_program_options.so)

# set source files
set(SOURCES src/main.cpp)

# set output folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY out/)

# set the name of the executable
add_executable(sota_client ${SOURCES})

# define used libraries for the used target
target_link_libraries(sota_client ${LINK_LIBS})

# enable cmakes test functionality
enable_testing()

add_test(NAME feat1_test--help
         COMMAND sota_client --help)         
add_test(NAME feat1_test-h
         COMMAND sota_client -h)
add_test(NAME feat1_test--something
         COMMAND sota_client --something)
add_test(NAME feat1_test-sth
         COMMAND sota_client -sth)
add_test(NAME feat1_test-no_opt
         COMMAND sota_client)