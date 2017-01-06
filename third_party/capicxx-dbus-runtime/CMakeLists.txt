# Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8.1)

PROJECT(libcommonapi-dbus)

# version of CommonAPI-DBus
SET( LIBCOMMONAPI_DBUS_MAJOR_VERSION 3 )
SET( LIBCOMMONAPI_DBUS_MINOR_VERSION 1 )
SET( LIBCOMMONAPI_DBUS_PATCH_VERSION 10 )

message(STATUS "Project name: ${PROJECT_NAME}")

set(COMPONENT_VERSION ${LIBCOMMONAPI_DBUS_MAJOR_VERSION}.${LIBCOMMONAPI_DBUS_MINOR_VERSION}.${LIBCOMMONAPI_DBUS_PATCH_VERSION})
set(COMMONAPI_API_HEADER_VERSION ${LIBCOMMONAPI_DBUS_MAJOR_VERSION}.${LIBCOMMONAPI_DBUS_MINOR_VERSION}) # used in *.cmake.in

SET(PACKAGE_VERSION "${COMPONENT_VERSION}") # used in *.cmake.in

set(GTEST_ROOT "n/a" CACHE STRING "Path to root folder of googletest. Must be set for building the tests.")

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

OPTION(USE_INSTALLED_COMMONAPI "Set to OFF to use the local (build tree) version of CommonAPI" ON)
message(STATUS "USE_INSTALLED_COMMONAPI is set to value: ${USE_INSTALLED_COMMONAPI}")

OPTION(USE_INSTALLED_DBUS "Set to OFF to use the local (build tree) version of DBUS" ON)
message(STATUS "USE_INSTALLED_DBUS is set to value: ${USE_INSTALLED_DBUS}")

OPTION(USE_FILE "Set to OFF to disable file logging" OFF )
message(STATUS "USE_FILE is set to value: ${USE_FILE}")

OPTION(USE_CONSOLE "Set to OFF to disable console logging" OFF )
message(STATUS "USE_CONSOLE is set to value: ${USE_CONSOLE}")

SET(DEFAULT_SEND_TIMEOUT "5000" CACHE STRING "default send timeout")
message(STATUS "DEFAULT_SEND_TIMEOUT is set to value: ${DEFAULT_SEND_TIMEOUT} ms")

SET(DBUS_DAEMON_PROXY_DEFAULT_SEND_TIMEOUT "10000" CACHE STRING "dbus daemon proxy default send timeout")
message(STATUS "DBUS_DAEMON_PROXY_DEFAULT_SEND_TIMEOUT is set to value: ${DBUS_DAEMON_PROXY_DEFAULT_SEND_TIMEOUT} ms")

SET(RPM_PACKAGE_VERSION "r0" CACHE STRING "rpm packet version") # used in e.g. commonapi-dbus.spec.in

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
       "Choose the type of build, options are: Debug Release." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_BUILD_TYPE_FOR_SPEC_IN "") # used in *.cmake.in
string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_FOR_SPEC_IN)

SET(BUILD_SERVER_COMMONAPI_INSTALL_PATH "na" CACHE STRING "CommonAPI install path on the build server")

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set (INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include/CommonAPI-${COMMONAPI_API_HEADER_VERSION} CACHE PATH "Installation directory for header files")
if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/cmake/CommonAPI-DBus-${COMPONENT_VERSION})
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Make relative paths absolute (needed later on)
foreach(p LIB INCLUDE CMAKE)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(ABSOLUTE_${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()

message(STATUS "RPM packet version set to ${RPM_PACKAGE_VERSION}")

SET(MAX_LOG_LEVEL "DEBUG" CACHE STRING "maximum log level")
message(STATUS "MAX_LOG_LEVEL is set to value: ${MAX_LOG_LEVEL}")

##################### RPM CONFIG ########################
SET( LICENSE "MPLv2" )
#########################################################

IF(USE_FILE)
  add_definitions(-DUSE_FILE)
ENDIF(USE_FILE)
IF(USE_CONSOLE)
  add_definitions(-DUSE_CONSOLE)
ENDIF(USE_CONSOLE)

add_definitions(-DDEFAULT_SEND_TIMEOUT=${DEFAULT_SEND_TIMEOUT})
add_definitions(-DDBUS_DAEMON_PROXY_DEFAULT_SEND_TIMEOUT=${DBUS_DAEMON_PROXY_DEFAULT_SEND_TIMEOUT})

add_definitions( -DCOMMONAPI_DBUS_VERSION_MAJOR=${LIBCOMMONAPI_DBUS_MAJOR_VERSION} )
add_definitions( -DCOMMONAPI_DBUS_VERSION_MINOR=${LIBCOMMONAPI_DBUS_MINOR_VERSION} )

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

##############################################################################
# add COMMONAPI install path to cmake find root path

if(NOT "${BUILD_SERVER_COMMONAPI_INSTALL_PATH}" STREQUAL "na")
  set( CMAKE_FIND_ROOT_PATH ${BUILD_SERVER_COMMONAPI_INSTALL_PATH};${CMAKE_FIND_ROOT_PATH} )
endif(NOT "${BUILD_SERVER_COMMONAPI_INSTALL_PATH}" STREQUAL "na")
message(STATUS "CMAKE_FIND_ROOT_PATH: ${CMAKE_FIND_ROOT_PATH}")

FIND_PACKAGE(PkgConfig)
FIND_PACKAGE(Threads REQUIRED)
if ("${USE_INSTALLED_COMMONAPI}" STREQUAL "ON")
    FIND_PACKAGE(CommonAPI 3.1.10 REQUIRED CONFIG NO_CMAKE_PACKAGE_REGISTRY)
else()
    FIND_PACKAGE(CommonAPI 3.1.10 REQUIRED CONFIG NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
endif()

message(STATUS "CommonAPI_CONSIDERED_CONFIGS: ${CommonAPI_CONSIDERED_CONFIGS}")
message(STATUS "COMMONAPI_INCLUDE_DIRS: ${COMMONAPI_INCLUDE_DIRS}")
message(STATUS "CommonAPI Version: ${CommonAPI_VERSION}")

###############################################################################
# find DBus by using the 'pkg-config' tool
if (MSVC)
    #Not beautiful, but it works
    if (DBus_DIR)
        if (DBus_BUILD_DIR)
            set(DBus_INCLUDE_DIRS "${DBus_DIR};${DBus_BUILD_DIR};")
            set(DBus_LIBRARY_RELEASE "${DBus_BUILD_DIR}/bin/Release/dbus-1.lib")
            set(DBus_LIBRARY_DEBUG "${DBus_BUILD_DIR}/bin/Debug/dbus-1d.lib")
        endif()
    endif()

    if (NOT DBus_LIBRARIES)
        include(SelectLibraryConfigurations)
        select_library_configurations(DBus)
    endif()
    if (NOT DBus_INCLUDE_DIRS OR NOT DBus_LIBRARIES)
      message(SEND_ERROR "Set DBus_INCLUDE_DIRS and DBus_LIBRARIES to your D-Bus installation (e.g. cmake -DDBus_INCLUDE_DIRS=C:\\D-Bus\\include -DDBus_LIBRARIES=C:\\DBus\\lib\\dbus-1.lib")
    endif()
else()
    pkg_check_modules(DBus REQUIRED dbus-1>=1.4)
endif()

##############################################################################

# CommonAPI-DBus build section

if (MSVC)
# Visual C++ is not always sure whether he is really C++
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_CRT_SECURE_NO_WARNINGS -DCOMMONAPI_INTERNAL_COMPILATION -DCOMMONAPI_DLL_COMPILATION /EHsc /wd4503")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS -DCOMMONAPI_INTERNAL_COMPILATION -DCOMMONAPI_DLL_COMPILATION /wd4503")
else()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Wextra -Wformat -Wformat-security -Wconversion -fexceptions -fstrict-aliasing -fstack-protector -fasynchronous-unwind-tables -fno-omit-frame-pointer -DCOMMONAPI_INTERNAL_COMPILATION -D_GLIBCXX_USE_NANOSLEEP")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOMMONAPI_LOGLEVEL=COMMONAPI_LOGLEVEL_${MAX_LOG_LEVEL}")

message(STATUS "Compiler options: ${CMAKE_CXX_FLAGS}") 

include_directories(
    include
    ${COMMONAPI_INCLUDE_DIRS}
    ${DBus_INCLUDE_DIRS}
)

if ("${USE_INSTALLED_DBUS}" STREQUAL "OFF")
	set (DBus_LIBRARIES dbus-1)
    link_directories(
        ${DBus_LIBRARY_DIRS}
    )
endif()

# DBus source files
file(GLOB CAPIDB_SRCS "src/CommonAPI/DBus/*.cpp")
list(SORT CAPIDB_SRCS)

# pugixml source files
file(GLOB PUGIXML_SRCS "src/pugixml/*.cpp")
list(SORT PUGIXML_SRCS)

# murmurhash source files
file(GLOB MMHASH_SRCS "src/murmurhash/*.cpp")
list(SORT MMHASH_SRCS)

# CommonAPI-DBus library
add_library(CommonAPI-DBus SHARED ${CAPIDB_SRCS} ${PUGIXML_SRCS} ${MMHASH_SRCS})

target_link_libraries(CommonAPI-DBus CommonAPI ${DBus_LIBRARIES})

if (MSVC)
    target_link_libraries(CommonAPI-DBus ws2_32 Rpcrt4)
endif()

set_target_properties(CommonAPI-DBus PROPERTIES VERSION ${LIBCOMMONAPI_DBUS_MAJOR_VERSION}.${LIBCOMMONAPI_DBUS_MINOR_VERSION}.${LIBCOMMONAPI_DBUS_PATCH_VERSION} SOVERSION ${LIBCOMMONAPI_DBUS_MAJOR_VERSION}.${LIBCOMMONAPI_DBUS_MINOR_VERSION}.${LIBCOMMONAPI_DBUS_PATCH_VERSION} LINKER_LANGUAGE C)

##############################################################################

# configure files

CONFIGURE_FILE(commonapi-dbus.spec.in commonapi-dbus.spec)

##############################################################################
# installing files

# for installation of CommonAPI header files
file (GLOB_RECURSE CommonAPI-DBus_INCLUDE_INSTALL_FILES "include/CommonAPI/DBus/*.hpp")
list (SORT CommonAPI-DBus_INCLUDE_INSTALL_FILES)
set_target_properties (CommonAPI-DBus PROPERTIES PUBLIC_HEADER "${CommonAPI-DBus_INCLUDE_INSTALL_FILES}")

# install CommonAPI-DBus library including headers
install(TARGETS CommonAPI-DBus
        EXPORT CommonAPI-DBusTargets
        RUNTIME DESTINATION ${INSTALL_BIN_DIR}
        LIBRARY DESTINATION ${INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/CommonAPI/DBus"
)

# install pugixml header files
install(FILES include/pugixml/pugiconfig.hpp DESTINATION ${INSTALL_INCLUDE_DIR}/pugixml)
install(FILES include/pugixml/pugixml.hpp DESTINATION ${INSTALL_INCLUDE_DIR}/pugixml)

##############################################################################
# exporting, configuring and installing of cmake files

# Add all targets to the build-tree export set
export(TARGETS CommonAPI-DBus
  FILE "${PROJECT_BINARY_DIR}/CommonAPI-DBusTargets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE CommonAPI-DBus)

# Create the CommonAPI-DBusConfig.cmake and CommonAPI-DBusConfigVersion files ...
file(RELATIVE_PATH REL_INCLUDE_DIR "${ABSOLUTE_INSTALL_CMAKE_DIR}" "${ABSOLUTE_INSTALL_INCLUDE_DIR}")

# ... for the build tree
set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonAPI-DBusConfig.cmake.in
  "${PROJECT_BINARY_DIR}/CommonAPI-DBusConfig.cmake" @ONLY)

# ... for the install tree
set(CONF_INCLUDE_DIRS "\${COMMONAPI_DBUS_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonAPI-DBusConfig.cmake.in
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CommonAPI-DBusConfig.cmake" @ONLY)

# ... for both
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonAPI-DBusConfigVersion.cmake.in
  "${PROJECT_BINARY_DIR}/CommonAPI-DBusConfigVersion.cmake" @ONLY)

# Install the CommonAPI-DBusConfig.cmake and CommonAPI-DBusConfigVersion.cmake
install(FILES
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CommonAPI-DBusConfig.cmake"
  "${PROJECT_BINARY_DIR}/CommonAPI-DBusConfigVersion.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}")

# Install the export set for use with the install-tree
install(EXPORT CommonAPI-DBusTargets DESTINATION
  "${INSTALL_CMAKE_DIR}")

##############################################################################
# maintainer-clean
add_custom_target(maintainer-clean COMMAND rm -rf *)

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
    set(PROJECT "GENIVI IPC Common API DBus C++")
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
                                   -o doc/html/README.html 
                                   ${PROJECT_BINARY_DIR}/../README)
    endif()
endif()

##############################################################################
# create pkg-config file
if(NOT WIN32 AND PKG_CONFIG_FOUND)
        configure_file(CommonAPI-DBus.pc.in ${PROJECT_BINARY_DIR}/CommonAPI-DBus.pc @ONLY)
        install(FILES ${PROJECT_BINARY_DIR}/CommonAPI-DBus.pc DESTINATION lib/pkgconfig)
endif()

##############################################################################
# Test section
##############################################################################

##############################################################################
# google test

# check for set environment variable
if(${GTEST_ROOT} STREQUAL "n/a")
    message(STATUS "GTEST_ROOT is not defined. For building the tests environment variable
             GTEST_ROOT has to be defined. Tests can not be built.")
    # early exit
    return()  # test can not be build -> make commands build_tests and check are not available
else()
    message(STATUS "GTEST_ROOT is set. gtest root path set to ${GTEST_ROOT}")
endif()

# build google test as static library (always) -> therefore deactivate BUILD_SHARED_LIBS in case it is active
set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)
if ("${BUILD_SHARED_LIBS}" STREQUAL "ON")
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 1)
endif()
add_subdirectory(${GTEST_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gtest EXCLUDE_FROM_ALL)
if ("${BUILD_SHARED_LIBS_AUTOMATIC_OFF}" STREQUAL "1")
    set(BUILD_SHARED_LIBS ON)
    set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)
endif()

##############################################################################
# build tests

enable_testing()

add_custom_target(build_tests)

set(CMAKE_CTEST_COMMAND ctest -V)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

add_dependencies(check build_tests)

##############################################################################
# add test directory

add_subdirectory( src/test EXCLUDE_FROM_ALL )

