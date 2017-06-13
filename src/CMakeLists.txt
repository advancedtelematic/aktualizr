# set symbols used when compiling
add_definitions(-DBOOST_LOG_DYN_LINK)

if(AKTUALIZR_VERSION)
    add_definitions(-DAKTUALIZR_VERSION="${AKTUALIZR_VERSION}")
else(AKTUALIZR_VERSION)
    add_definitions(-DAKTUALIZR_VERSION="dev")
endif(AKTUALIZR_VERSION)

# find all required libraries
find_package(Boost COMPONENTS system filesystem thread program_options log log_setup regex chrono date_time REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_search_module(SODIUM libsodium REQUIRED)

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
            src/crypto.cc
            src/utils.cc)
# set headers used for clang format
set(HEADERS
            src/logger.h
            src/httpclient.h
            src/sotahttpclient.h
            src/sotauptaneclient.h
            src/config.h
            src/channel.h
            src/events.h
            src/commands.h
            src/types.h
            src/eventsinterpreter.h
            src/gatewaymanager.h
            src/socketgateway.h
            src/crypto.h
            src/openssl_compat.h
            src/ostree.h
            src/uptane/tufrepository.h
            src/uptane/uptanerepository.h
            src/uptane/exceptions.h)

set(MAIN src/main.cc)

if(BUILD_GENIVI)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(LIBDBUS REQUIRED dbus-1)
    list(APPEND SOURCES src/dbusgateway/dbusgateway.cc
                        src/dbusgateway/swlm.cc
                        src/sotarviclient.cc)
    list(APPEND HEADERS src/dbusgateway/dbusgateway.h
                        src/dbusgateway/swlm.h)
endif(BUILD_GENIVI)

if (BUILD_OSTREE)
    list(APPEND SOURCES src/ostree.cc
                        src/sotauptaneclient.cc
                        src/uptane/tufrepository.cc
                        src/uptane/uptanerepository.cc)
endif(BUILD_OSTREE)

include_directories(src)
include_directories(third_party/jsoncpp)
include_directories(third_party/picojson)

# set the name of the executable
add_executable(aktualizr ${MAIN})
add_library(aktualizr_static_lib STATIC ${SOURCES})

if(BUILD_OSTREE)
    find_package(OSTree REQUIRED)
    target_link_libraries(aktualizr_static_lib ${LIBOSTREE_LIBRARIES})
    target_include_directories(aktualizr_static_lib PUBLIC ${LIBOSTREE_INCLUDE_DIRS})
    add_definitions(-DBUILD_OSTREE)
endif(BUILD_OSTREE)


set_property(SOURCE third_party/jsoncpp/jsoncpp.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -w")


target_link_libraries(aktualizr aktualizr_static_lib
    ${Boost_LIBRARIES} ${CURL_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${SODIUM_LIBRARIES})

if(BUILD_GENIVI)
    set_property(TARGET aktualizr PROPERTY CXX_STANDARD 11)
    add_definitions(-DWITH_GENIVI)
    target_compile_options(aktualizr_static_lib PUBLIC ${LIBDBUS_CFLAGS})
    target_include_directories(aktualizr_static_lib PUBLIC ${LIBDBUS_INCLUDE_DIRS})
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


################## INSTALL RULES
#configure_file(distribution/sota.service.cmake distribution/sota.service)
#install(FILES distribution/sota.service DESTINATION /etc/systemd/system  COMPONENT init)
install(TARGETS aktualizr RUNTIME DESTINATION bin)
if(INSTALL_LIB)
  install(TARGETS aktualizr_static_lib ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
  install(FILES ${HEADERS} DESTINATION include)
  install(DIRECTORY third_party/jsoncpp/json DESTINATION include)
  install(DIRECTORY third_party/picojson DESTINATION include)
  install(DIRECTORY src/uptane DESTINATION include FILES_MATCHING PATTERN "*.h")
endif()
# vim: set tabstop=4 shiftwidth=4 expandtab:
