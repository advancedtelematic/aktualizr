# set symbols used when compiling
add_definitions(-DBOOST_LOG_DYN_LINK)

# find all required libraries
find_package(Boost COMPONENTS system thread program_options log log_setup regex chrono REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# set source files excluded main for using the list for the test target
set(SOURCES third_party/jsoncpp/jsoncpp.cpp
            src/logger.cc
            src/httpclient.cc
            src/sotahttpclient.cc
            src/config.cc
            src/events.cc
            src/commands.cc
            src/types.cc
            src/eventsinterpreter.cc
            src/gatewaymanager.cc
            src/socketgateway.cc
            src/crypto.cc)
# set headers used for clang format
set(HEADERS
            src/logger.h
            src/httpclient.h
            src/sotahttpclient.h
            src/uptanehttpclient.h
            src/config.h
            src/events.h
            src/commands.h
            src/types.h
            src/eventsinterpreter.h
            src/gatewaymanager.h
            src/socketgateway.h
            src/uptane.h
            src/crypto.h
            src/ostree.h)

set(MAIN src/main.cc)
            
if(BUILD_GENIVI)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(LIBDBUS REQUIRED dbus-1)
    list(APPEND SOURCES src/dbusgateway/dbusgateway.cc
                        src/dbusgateway/swlm.cc
                        src/sotarviclient.cc)
endif(BUILD_GENIVI)

include_directories(src)
include_directories(third_party/jsoncpp)
include_directories(third_party/picojson)

# set the name of the executable
add_executable(aktualizr ${MAIN})

if(BUILD_OSTREE)
    list(APPEND SOURCES src/sotauptaneclient.cc
                        src/ostree.cc)
    pkg_search_module(LIBGLIB REQUIRED gio-2.0)
    include_directories(third_party/ostree/include/ostree-1)
    link_directories(${CMAKE_SOURCE_DIR}/third_party/ostree/lib)
    include_directories(${LIBGLIB_INCLUDE_DIRS})
    set(OSTREE_LIBS gio-2.0 glib-2.0 ostree-1)

    target_link_libraries(aktualizr ${OSTREE_LIBS})
endif(BUILD_OSTREE)


set_property(SOURCE third_party/jsoncpp/jsoncpp.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -w")


add_library(aktualizr_static_lib STATIC ${SOURCES})


target_link_libraries(aktualizr aktualizr_static_lib ${Boost_LIBRARIES} ${CURL_LIBRARIES} ${OpenSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OSTREE_LIBS})

if(BUILD_GENIVI)
    set_property(TARGET aktualizr PROPERTY CXX_STANDARD 11)
    add_definitions(-DWITH_GENIVI)
    target_compile_options(aktualizr_static_lib PUBLIC ${LIBDBUS_CFLAGS})
    target_include_directories(aktualizr_static_lib PUBLIC third_party/rvi_lib/include)
    target_link_libraries(aktualizr ${LIBDBUS_LIBRARIES} rvi)
endif(BUILD_GENIVI)


if(CLANG_FORMAT)
    add_custom_target(format-src
        COMMAND ${CLANG_FORMAT} -i -style=file ${SOURCES} ${HEADERS} src/main.cc
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on src"
        VERBATIM)
    add_dependencies(qa format-src)
else()
    message(WARNING "clang-format not found, skipping")
endif()

if(CLANG_CHECK)
    add_custom_target(check-src
        COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} ${SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-check"
        VERBATIM)
    add_dependencies(qa check-src aktualizr)
else()
    message(WARNING "clang-check not found, skipping")
endif(CLANG_CHECK)
if(BUILD_OSTREE)
    include(ExternalProject)
    ExternalProject_Add(ostree
         PREFIX "third_party/ostree"
         GIT_REPOSITORY "https://github.com/ostreedev/ostree.git"
         CONFIGURE_COMMAND env NOCONFIGURE=1 ./autogen.sh && ./configure --prefix=<INSTALL_DIR>
         BUILD_COMMAND ${MAKE}
         BUILD_IN_SOURCE 1)
    add_dependencies(ostree aktualizr)
    add_definitions(-DBUILD_OSTREE)
endif()


################## INSTALL RULES
#configure_file(distribution/sota.service.cmake distribution/sota.service)
#install(FILES distribution/sota.service DESTINATION /etc/systemd/system  COMPONENT init)
install(TARGETS aktualizr RUNTIME  DESTINATION bin)
# vim: set tabstop=4 shiftwidth=4 expandtab:
