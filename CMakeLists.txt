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

#add_subdirectory("src")
# FIXME temporary use include because of bugs with coverage
include("src/CMakeLists.txt")
include("tests/CMakeLists.txt")
