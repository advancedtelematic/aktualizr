set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS -w)
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/p11engine.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

add_subdirectory("uptane")

# set main aktualizr source files
set(AKTUALIZR_SOURCES aktualizr.cc
            bootstrap.cc
            commands.cc
            config.cc
            events.cc
            eventsinterpreter.cc
            gatewaymanager.cc
            ipsecondarydiscovery.cc
            socketgateway.cc
            sotauptaneclient.cc
            timer.cc
            )

# set headers used for clang format
set(HEADERS aktualizr.h
            aktualizr_secondary_ipc.h
            asn1-cer.h
            asn1-cerstream.h
            bootstrap.h
            channel.h
            commands.h
            config.h
            config_utils.h
            crypto.h
            deb.h
            events.h
            eventsinterpreter.h
            exceptions.h
            fsstorage.h
            gateway.h
            gatewaymanager.h
            httpclient.h
            httpinterface.h
            invstorage.h
            ipsecondarydiscovery.h
            ipuptaneconnection.h
            ipuptaneconnectionsplitter.h
            keymanager.h
            logging.h
            openssl_compat.h
            ostree.h
            ostreereposync.h
            p11engine.h
            packagemanagerfactory.h
            packagemanagerfake.h
            packagemanagerinterface.h
            socketgateway.h
            sotauptaneclient.h
            sqlstorage.h
            sql_utils.h
            timer.h
            types.h
            utils.h)

set(AKTUALIZR_SECONDARY_IPC_SOURCES
    aktualizr_secondary_ipc.cc
    asn1-cer.cc
    asn1-cerstream.cc
    ipuptaneconnection.cc
    ipuptaneconnectionsplitter.cc)
add_library(aktualizr_secondary_ipc OBJECT ${AKTUALIZR_SECONDARY_IPC_SOURCES})
target_include_directories(aktualizr_secondary_ipc PUBLIC
    $<TARGET_PROPERTY:socket_activation,INCLUDE_DIRECTORIES>)

set(COMMON_UTILITIES_SOURCES
    crypto.cc
    httpclient.cc
    keymanager.cc
    types.cc
    logging.cc
    utils.cc)
add_library(common_utilities OBJECT ${COMMON_UTILITIES_SOURCES})

set(PACMAN_FAKE_SOURCES packagemanagerfake.cc)
add_library(pacman OBJECT ${PACMAN_FAKE_SOURCES})
set(DEB_SOURCES deb.cc)
if(BUILD_DEB)
    target_sources(pacman PUBLIC ${DEB_SOURCES})
endif(BUILD_DEB)

set(OSTREE_SOURCES ostree.cc ostreereposync.cc)
if(BUILD_OSTREE)
    target_sources(pacman PUBLIC ${OSTREE_SOURCES})
    target_include_directories(pacman PUBLIC ${LIBOSTREE_INCLUDE_DIRS})
endif(BUILD_OSTREE)

set(P11_SOURCES p11engine.cc)
if(BUILD_P11)
    target_sources(common_utilities PUBLIC ${P11_SOURCES})

    if(TEST_PKCS11_MODULE_PATH)
        add_definitions(-DTEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}" -DTEST_PKCS11_ENGINE_PATH="${TEST_PKCS11_ENGINE_PATH}")
    endif(TEST_PKCS11_MODULE_PATH)
endif(BUILD_P11)

set(STORAGE_SOURCES fsstorage.cc sqlstorage.cc invstorage.cc)
add_library(storage OBJECT ${STORAGE_SOURCES})

add_library(jsoncpp OBJECT ${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp)

# set the name of the executable
add_library(aktualizr_static_lib STATIC
    ${AKTUALIZR_SOURCES}
    $<TARGET_OBJECTS:jsoncpp>
    $<TARGET_OBJECTS:pacman>
    $<TARGET_OBJECTS:common_utilities>
    $<TARGET_OBJECTS:aktualizr_secondary_ipc>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:uptane>
    $<TARGET_OBJECTS:socket_activation>)

target_include_directories(aktualizr_static_lib PUBLIC
    $<TARGET_PROPERTY:pacman,INCLUDE_DIRECTORIES>)

set(OPCUABRIDGE_SOURCES opcuabridge/common.cc
                        opcuabridge/filelist.cc
                        opcuabridge/opcuabridgeclient.cc
                        opcuabridge/opcuabridgeserver.cc
                        opcuabridge/opcuabridgediscoveryclient.cc
                        opcuabridge/opcuabridgediscoveryserver.cc
                        uptane/opcuasecondary.cc)
set(OPCUABRIDGE_HEADERS opcuabridge/boostarch.h
                        opcuabridge/common.h
                        opcuabridge/configuration.h
                        opcuabridge/currenttime.h
                        opcuabridge/ecuversionmanifest.h
                        opcuabridge/ecuversionmanifestsigned.h
                        opcuabridge/filedata.h
                        opcuabridge/filelist.h
                        opcuabridge/hash.h
                        opcuabridge/imageblock.h
                        opcuabridge/imagefile.h
                        opcuabridge/image.h
                        opcuabridge/imagerequest.h
                        opcuabridge/metadatafile.h
                        opcuabridge/metadatafiles.h
                        opcuabridge/opcuabridge.h
                        opcuabridge/opcuabridgeclient.h
                        opcuabridge/opcuabridgeconfig.h
                        opcuabridge/opcuabridgediscoveryclient.h
                        opcuabridge/opcuabridgediscoveryserver.h
                        opcuabridge/opcuabridgediscoverytypes.h
                        opcuabridge/opcuabridgeserver.h
                        opcuabridge/originalmanifest.h
                        opcuabridge/signature.h
                        opcuabridge/signed.h
                        opcuabridge/utility.h
                        opcuabridge/versionreport.h
                        uptane/opcuasecondary.h)
if (BUILD_OPCUA)
    set(OPEN62541_SOURCES ${PROJECT_SOURCE_DIR}/third_party/open62541/open62541.c)
    set_source_files_properties(${OPEN62541_SOURCES} ${OPCUABRIDGE_SOURCES}
        PROPERTIES COMPILE_FLAGS ${OPEN62541_IGNORED_WARNINGS})
    add_library(opcua_bridge OBJECT ${OPCUABRIDGE_SOURCES} ${OPEN62541_SOURCES})
    target_sources(aktualizr_static_lib PUBLIC $<TARGET_OBJECTS:opcua_bridge>)
endif (BUILD_OPCUA)

add_executable(aktualizr main.cc)
target_link_libraries(aktualizr aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${sodium_LIBRARY_RELEASE}
        ${LIBOSTREE_LIBRARIES}
        ${LIBP11_LIBRARIES}
        ${SQLITE3_LIBRARIES}
        ${LibArchive_LIBRARIES}
        ${SYSTEMD_LIBRARY}
        ${LIBDPKG_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr gcov)
endif(BUILD_WITH_CODE_COVERAGE)

################ QA RULES

set(ALL_CHECKS main.cc
    ${AKTUALIZR_SOURCES}
    ${AKTUALIZR_SECONDARY_IPC_SOURCES}
    ${COMMON_UTILITIES_SOURCES}
    ${DEB_SOURCES}
    ${OSTREE_SOURCES}
    ${P11_SOURCES}
    ${PACMAN_FAKE_SOURCES}
    ${STORAGE_SOURCES}
    ${OPCUABRIDGE_SOURCES} ${OPCUABRIDGE_HEADERS}
    ${HEADERS})
list(REMOVE_DUPLICATES ALL_CHECKS)
aktualizr_source_file_checks(${ALL_CHECKS})

################## INSTALL RULES
install(TARGETS aktualizr RUNTIME DESTINATION bin COMPONENT aktualizr)
if(INSTALL_LIB)
  install(TARGETS aktualizr_static_lib ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
  install(FILES ${HEADERS} DESTINATION include)
  install(DIRECTORY ../third_party/jsoncpp/json DESTINATION include)
  install(DIRECTORY ../third_party/picojson DESTINATION include)
  install(DIRECTORY uptane DESTINATION include FILES_MATCHING PATTERN "*.h")
endif()

# vim: set tabstop=4 shiftwidth=4 expandtab:
