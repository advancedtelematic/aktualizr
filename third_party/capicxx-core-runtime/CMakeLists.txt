# Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8.12)

PROJECT(libcommonapi)

# version of CommonAPI
SET( LIBCOMMONAPI_MAJOR_VERSION 3 )
SET( LIBCOMMONAPI_MINOR_VERSION 1 )
SET( LIBCOMMONAPI_PATCH_VERSION 10 )

message(STATUS "Project name: ${PROJECT_NAME}")

set(COMPONENT_VERSION ${LIBCOMMONAPI_MAJOR_VERSION}.${LIBCOMMONAPI_MINOR_VERSION}.${LIBCOMMONAPI_PATCH_VERSION})
set(COMMONAPI_API_HEADER_VERSION ${LIBCOMMONAPI_MAJOR_VERSION}.${LIBCOMMONAPI_MINOR_VERSION}) # used in *.cmake.in

SET(PACKAGE_VERSION "${COMPONENT_VERSION}") # used in *.cmake.in
message(STATUS "This is CMake for Common API C++ Version ${COMPONENT_VERSION}.")

# OS
set(DL_LIBRARY "")
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(DL_LIBRARY "dl")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

##############################################################################

# define standard installation path in case CMAKE_INSTALL_PREFIX is not set
if ("${CMAKE_INSTALL_PREFIX}" STREQUAL "")
  SET(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
message(STATUS "CMAKE_INSTALL_PREFIX set to: ${CMAKE_INSTALL_PREFIX}")

# create the commandline variables:
OPTION(BUILD_SHARED_LIBS "Set to OFF to build static libraries" ON )
message(STATUS "BUILD_SHARED_LIBS is set to value: ${BUILD_SHARED_LIBS}")
SET(RPM_PACKAGE_VERSION "r0" CACHE STRING "rpm packet version") # used in e.g. commonapi.spec.in

SET(MAX_LOG_LEVEL "DEBUG" CACHE STRING "maximum log level")
message(STATUS "MAX_LOG_LEVEL is set to value: ${MAX_LOG_LEVEL}")

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
       "Choose the type of build, options are: Debug Release." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_BUILD_TYPE_FOR_SPEC_IN "")
string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_FOR_SPEC_IN)

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for binaries")
set(INSTALL_INCLUDE_DIR include/CommonAPI-${COMMONAPI_API_HEADER_VERSION} CACHE PATH "Installation directory for header files")

if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/cmake/CommonAPI-${COMPONENT_VERSION})
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

OPTION(USE_FILE "Set to OFF to disable file logging" OFF )
message(STATUS "USE_FILE is set to value: ${USE_FILE}")

OPTION(USE_CONSOLE "Set to OFF to disable console logging" OFF )
message(STATUS "USE_CONSOLE is set to value: ${USE_CONSOLE}")

# Make relative paths absolute (needed later on)
foreach(p LIB INCLUDE CMAKE)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(ABSOLUTE_${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()

message(STATUS "RPM packet version set to ${RPM_PACKAGE_VERSION}")
##################### RPM CONFIG ########################
SET( LICENSE "MPLv2" )
#########################################################

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

##############################################################################
# CommonAPI build section

IF(USE_FILE)
  add_definitions(-DUSE_FILE)
ENDIF(USE_FILE)
IF(USE_CONSOLE)
  add_definitions(-DUSE_CONSOLE)
ENDIF(USE_CONSOLE)

IF(MSVC)
    message("using MSVC Compiler")
    add_definitions(-DCOMMONAPI_INTERNAL_COMPILATION -DCOMMONAPI_DLL_COMPILATION)
    add_compile_options(/EHsc /wd4996)
ELSE ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Wextra -Wformat -Wformat-security -Wconversion -fexceptions -fstrict-aliasing -fstack-protector -fasynchronous-unwind-tables -fno-omit-frame-pointer -Werror -DCOMMONAPI_INTERNAL_COMPILATION")
ENDIF(MSVC)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOMMONAPI_LOGLEVEL=COMMONAPI_LOGLEVEL_${MAX_LOG_LEVEL}")
 
FIND_PACKAGE(PkgConfig)
pkg_check_modules(DLT "automotive-dlt >= 2.11")
IF(DLT_FOUND) 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_DLT")
ENDIF(DLT_FOUND)

##############################################################################

include_directories(
    include
    ${DLT_INCLUDE_DIRS}
)

link_directories(
    ${DLT_LIBDIR}
)

file(GLOB CAPI_SRCS "src/CommonAPI/*.cpp")
list(SORT CAPI_SRCS)
add_library(CommonAPI SHARED ${CAPI_SRCS})
target_link_libraries(CommonAPI PRIVATE ${DL_LIBRARY} ${DLT_LIBRARIES})
set_target_properties(CommonAPI PROPERTIES VERSION ${LIBCOMMONAPI_MAJOR_VERSION}.${LIBCOMMONAPI_MINOR_VERSION}.${LIBCOMMONAPI_PATCH_VERSION} SOVERSION ${LIBCOMMONAPI_MAJOR_VERSION}.${LIBCOMMONAPI_MINOR_VERSION}.${LIBCOMMONAPI_PATCH_VERSION} LINKER_LANGUAGE C)
set_target_properties (CommonAPI PROPERTIES INTERFACE_LINK_LIBRARY "")

##############################################################################
# configure files

CONFIGURE_FILE(commonapi.spec.in commonapi.spec)

##############################################################################
# installing files

# for installation of CommonAPI header files
file (GLOB_RECURSE CommonAPI_INCLUDE_INSTALL_FILES "include/CommonAPI/*.hpp")
list (SORT CommonAPI_INCLUDE_INSTALL_FILES)
set_target_properties (CommonAPI PROPERTIES PUBLIC_HEADER "${CommonAPI_INCLUDE_INSTALL_FILES}")

# install CommonAPI library including headers
install(TARGETS CommonAPI
        EXPORT CommonAPITargets
        LIBRARY DESTINATION ${INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${INSTALL_BIN_DIR}
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/CommonAPI"
)

##############################################################################
# exporting, configuring and installing of cmake files

# Add all targets to the build-tree export set
export(TARGETS CommonAPI
  FILE "${PROJECT_BINARY_DIR}/CommonAPITargets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE CommonAPI)

# Create the CommonAPIConfig.cmake and CommonAPIConfigVersion files ...
file(RELATIVE_PATH REL_INCLUDE_DIR "${ABSOLUTE_INSTALL_CMAKE_DIR}" "${ABSOLUTE_INSTALL_INCLUDE_DIR}")

# ... for the build tree
set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include" )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonAPIConfig.cmake.in
  "${PROJECT_BINARY_DIR}/CommonAPIConfig.cmake" @ONLY)

# ... for the install tree
set(CONF_INCLUDE_DIRS "\${COMMONAPI_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonAPIConfig.cmake.in
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CommonAPIConfig.cmake" @ONLY)

# ... for both
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonAPIConfigVersion.cmake.in
  "${PROJECT_BINARY_DIR}/CommonAPIConfigVersion.cmake" @ONLY)

# Install the CommonAPIConfig.cmake and CommonAPIConfigVersion.cmake
install(FILES
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CommonAPIConfig.cmake"
  "${PROJECT_BINARY_DIR}/CommonAPIConfigVersion.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}")

# Install the export set for use with the install-tree
install(EXPORT CommonAPITargets DESTINATION
  "${INSTALL_CMAKE_DIR}")

##############################################################################
# maintainer-clean
add_custom_target(maintainer-clean
   COMMAND rm -rf *
)

##############################################################################
# dist
add_custom_target(dist
   COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && git archive --format=tar.gz ${COMPONENT_VERSION} -o CommonAPI${COMPONENT_VERSION}.tar.gz
)

##############################################################################
# build documentation
FIND_PACKAGE(Doxygen)
FIND_PROGRAM(ASCIIDOC_PATH asciidoc)
if (NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen is not installed. Documentation can not be built.")
else()
    # set configuration variables for doxygen.in
    set(PROJECT "Common API C++")
    set(DOCDIR doc)
    set(SRCDIR .)
    set(GENERATE_HTML YES)
    set(GENERATE_HTMLHELP NO)
    set(GENERATE_CHI NO)
    set(GENERATE_LATEX NO)
    set(GENERATE_PDF NO)
    set(GENERATE_RTF NO)
    set(GENERATE_MAN NO)
    set(GENERATE_XML NO)

    configure_file(doxygen.in ${PROJECT_BINARY_DIR}/Doxyfile @ONLY)

    add_custom_target(doc)

    add_custom_target(doxygen-doc
                      COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
                      SOURCES ${PROJECT_BINARY_DIR}/Doxyfile)

    add_dependencies(doc doxygen-doc)

    if ("${ASCIIDOC_PATH}" STREQUAL "ASCIIDOC_PATH-NOTFOUND")
        message(STATUS "asciidoc is not installed. Readme can not be built.")
    else()
        message(STATUS "asciidoc found")
        add_custom_command(TARGET doc
                           POST_BUILD
                           COMMAND asciidoc
                                   -a version=${PACKAGE_VERSION}
                                   -b html
                                   -o doc/README.html
                                   ${PROJECT_BINARY_DIR}/../INSTALL)
    endif()
endif()

##############################################################################
# create pkg-config file
if(NOT WIN32 AND PKG_CONFIG_FOUND)
        configure_file(CommonAPI.pc.in ${PROJECT_BINARY_DIR}/CommonAPI.pc @ONLY)
        install(FILES ${PROJECT_BINARY_DIR}/CommonAPI.pc DESTINATION lib/pkgconfig)
endif()

##############################################################################

