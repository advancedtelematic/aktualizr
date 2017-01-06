cmake_minimum_required (VERSION 2.6)

project(sota_client_cpp)

option(WARNING_AS_ERROR "Treat warnings as errors" ON)
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

OPTION(BUILD_WITH_DBUS_GATEWAY "Set to ON to compile with dbus gateway support" OFF)
message(STATUS "BUILD_WITH_DBUS_GATEWAY is set to value: ${BUILD_WITH_DBUS_GATEWAY}")
if(BUILD_WITH_DBUS_GATEWAY)

add_definitions(-DWITH_DBUS)

include(ExternalProject)

ExternalProject_Add(libdbus
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dbus-1.9.0
    CONFIGURE_COMMAND  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dbus-1.9.0/autogen.sh COMMAND  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dbus-1.9.0/configure --prefix=${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build --with-systemdsystemunitdir=${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build/lib/systemd
    BUILD_COMMAND ${MAKE}
    BUILD_IN_SOURCE 1)

ExternalProject_Add(commonapi
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/common-api-runtime
    CONFIGURE_COMMAND cmake ${CMAKE_CURRENT_SOURCE_DIR}/third_party/common-api-runtime/ -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_SOURCE_DIR}/commonapi_install
    BUILD_COMMAND ${MAKE}
    BUILD_IN_SOURCE 1)

ExternalProject_Add(dbusapi
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/common-api-dbus-runtime
    CONFIGURE_COMMAND PKG_CONFIG_PATH=${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build/lib/pkgconfig cmake ${CMAKE_CURRENT_SOURCE_DIR}/third_party/common-api-dbus-runtime/  -DUSE_INSTALLED_COMMONAPI=OFF  -DUSE_INSTALLED_DBUS=OFF -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_SOURCE_DIR}/dbusapi_install
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
