cmake_minimum_required (VERSION 2.6)

project(sota_client_cpp)

option(WARNING_AS_ERROR "Treat warnings as errors" ON)
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)

IF( NOT CMAKE_BUILD_TYPE )
    set(CMAKE_BUILD_TYPE Debug)
ENDIF()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

OPTION(BUILD_WITH_DBUS_GATEWAY "Set to ON to compile with dbus gateway support" OFF)
message(STATUS "BUILD_WITH_DBUS_GATEWAY is set to value: ${BUILD_WITH_DBUS_GATEWAY}")
if(BUILD_WITH_DBUS_GATEWAY)

add_definitions(-DWITH_DBUS)

include(ExternalProject)

ExternalProject_Add(libdbus
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dbus-1.9.0
    CONFIGURE_COMMAND  ./autogen.sh --prefix=${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build  COMMAND cmake cmake -DDBUS_INSTALL_DIR=${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build 
    BUILD_COMMAND ${MAKE}
    INSTALL_COMMAND make install COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dbus-1.9.0/dbus-1.pc ${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build/include ${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build/include/dbus-1.0
	BUILD_IN_SOURCE 1)

ExternalProject_Add(commonapi
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/common-api-runtime
    CONFIGURE_COMMAND cmake ${CMAKE_CURRENT_SOURCE_DIR}/third_party/common-api-runtime/ -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_SOURCE_DIR}/commonapi_install
    BUILD_COMMAND ${MAKE}
    BUILD_IN_SOURCE 1)

ExternalProject_Add(dbusapi
    PREFIX "third_party/common-api-dbus-runtime"
    GIT_REPOSITORY "http://git.projects.genivi.org/ipc/common-api-dbus-runtime.git"
    CONFIGURE_COMMAND PKG_CONFIG_PATH=${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build/ cmake . -DUSE_INSTALLED_COMMONAPI=OFF  -DUSE_INSTALLED_DBUS=OFF -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/third_party/common-api-dbus-runtime/patches/interface_and_session.patch
    BUILD_COMMAND ${MAKE}
    BUILD_IN_SOURCE 1)

endif(BUILD_WITH_DBUS_GATEWAY)


#add_subdirectory("src")
# FIXME temporary use include because of bugs with coverage
include("src/CMakeLists.txt")
include("tests/CMakeLists.txt")

# Generate ctags
set_source_files_properties(tags PROPERTIES GENERATED true)
add_custom_target(tags
    COMMAND ctags -R --c++-kinds=+p --fields=+iaS --extra=+q src
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
