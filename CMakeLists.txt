cmake_minimum_required (VERSION 2.6)

project(aktualizr)

option(WARNING_AS_ERROR "Treat warnings as errors" ON)
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)

IF( NOT CMAKE_BUILD_TYPE )
    set(CMAKE_BUILD_TYPE Debug)
ENDIF()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

OPTION(BUILD_WITH_DBUS_GATEWAY "Set to ON to compile with dbus gateway support" OFF)
message(STATUS "BUILD_WITH_DBUS_GATEWAY is set to value: ${BUILD_WITH_DBUS_GATEWAY}")

include(ExternalProject)

if(BUILD_WITH_DBUS_GATEWAY)
    add_definitions(-DWITH_DBUS)
endif(BUILD_WITH_DBUS_GATEWAY)

ExternalProject_Add(rvi_lib
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rvi_lib
    CONFIGURE_COMMAND  autoreconf -i  COMMAND ./configure 
    INSTALL_COMMAND ""
	BUILD_COMMAND ${MAKE}
    BUILD_IN_SOURCE 1)


#add_subdirectory("src")
# FIXME temporary use include because of bugs with coverage
include("src/CMakeLists.txt")
include("tests/CMakeLists.txt")

# Generate ctags
set_source_files_properties(tags PROPERTIES GENERATED true)
add_custom_target(tags
    COMMAND ctags -R --c++-kinds=+p --fields=+iaS --extra=+q src
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
