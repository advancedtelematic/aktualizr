set(AKTUALIZR_SECONDARY_SRC main.cc)

set(AKTUALIZR_SECONDARY_LIB_SRC
    aktualizr_secondary.cc
    aktualizr_secondary_config.cc
    aktualizr_secondary_common.cc
    aktualizr_secondary_discovery.cc
    )

set(OPCUA_SECONDARY_SRC
    opcuaserver_secondary_delegate.cc
    aktualizr_secondary_opcua.cc
    )

if (BUILD_OPCUA)
    set_source_files_properties(${OPCUA_SECONDARY_SRC} ${AKTUALIZR_SECONDARY_SRC}
        PROPERTIES COMPILE_FLAGS ${OPEN62541_IGNORED_WARNINGS})
    list(APPEND AKTUALIZR_SECONDARY_LIB_SRC ${OPCUA_SECONDARY_SRC})
endif (BUILD_OPCUA)

add_library(aktualizr_secondary_static_lib STATIC ${AKTUALIZR_SECONDARY_LIB_SRC})
target_link_libraries(aktualizr_secondary_static_lib
    socket_activation)

add_executable(aktualizr-secondary ${AKTUALIZR_SECONDARY_SRC})
target_link_libraries(aktualizr-secondary
    aktualizr_secondary_static_lib
    aktualizr_static_lib
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${sodium_LIBRARY_RELEASE}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CURL_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${LibArchive_LIBRARIES}
    ${LIBOSTREE_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${LIBDPKG_LIBRARIES}
    ${LIBP11_LIBRARIES}
    gcov
    )

install(TARGETS aktualizr-secondary
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

set(ALL_AKTUALIZR_SECONDARY_HEADERS
    aktualizr_secondary.h
    aktualizr_secondary_config.h
    aktualizr_secondary_common.h
    aktualizr_secondary_discovery.h
    aktualizr_secondary_opcua.h
    opcuaserver_secondary_delegate.h
    )

set(AKTUALIZR_SECONDARY_ALL_CHECKS ${AKTUALIZR_SECONDARY_SRC} ${AKTUALIZR_SECONDARY_LIB_SRC} ${ALL_AKTUALIZR_SECONDARY_HEADERS}
                                   ${OPCUA_SECONDARY_SRC})
list(REMOVE_DUPLICATES AKTUALIZR_SECONDARY_ALL_CHECKS)
aktualizr_source_file_checks(${AKTUALIZR_SECONDARY_ALL_CHECKS})

# vim: set tabstop=4 shiftwidth=4 expandtab:
