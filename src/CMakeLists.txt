set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS -w)
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/p11engine.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)


# set source files excluding main
set(SOURCES aktualizr.cc
            aktualizr_secondary_ipc.cc
            asn1-cer.cc
            asn1-cerstream.cc
            bootstrap.cc
            commands.cc
            config.cc
            crypto.cc
            events.cc
            eventsinterpreter.cc
            fsstorage.cc
            gatewaymanager.cc
            httpclient.cc
            invstorage.cc
            ipsecondarydiscovery.cc
            ipuptaneconnection.cc
            ipuptaneconnectionsplitter.cc
            keymanager.cc
            logging.cc
            socketgateway.cc
            sotauptaneclient.cc
            sqlstorage.cc
            timer.cc
            types.cc
            utils.cc
            uptane/initialize.cc
            uptane/ipuptanesecondary.cc
            uptane/legacysecondary.cc
            uptane/managedsecondary.cc
            uptane/partialverificationsecondary.cc
            uptane/role.cc
            uptane/root.cc
            uptane/tuf.cc
            uptane/uptanerepository.cc
            uptane/virtualsecondary.cc)

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
            utils.h
            uptane/exceptions.h
            uptane/ipuptanesecondary.h
            uptane/legacysecondary.h
            uptane/managedsecondary.h
            uptane/partialverificationsecondary.h
            uptane/secondaryconfig.h
            uptane/secondaryfactory.h
            uptane/secondaryinterface.h
            uptane/tuf.h
            uptane/uptanerepository.h
            uptane/virtualsecondary.h)

list(APPEND OSTREE_SOURCES ostree.cc ostreereposync.cc)
if(BUILD_OSTREE)
    list(APPEND SOURCES ${OSTREE_SOURCES})
endif(BUILD_OSTREE)

list(APPEND DEB_SOURCES deb.cc)
if(BUILD_DEB)
    list(APPEND SOURCES ${DEB_SOURCES})
endif(BUILD_DEB)

list(APPEND P11_SOURCES p11engine.cc)
if(BUILD_P11)
    list(APPEND SOURCES ${P11_SOURCES})

    if(TEST_PKCS11_MODULE_PATH)
        add_definitions(-DTEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}" -DTEST_PKCS11_ENGINE_PATH="${TEST_PKCS11_ENGINE_PATH}")
    endif(TEST_PKCS11_MODULE_PATH)
endif(BUILD_P11)

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
                        opcuabridge/signature.h
                        opcuabridge/signed.h
                        opcuabridge/utility.h
                        opcuabridge/versionreport.h
                        uptane/opcuasecondary.h)
if (BUILD_OPCUA)
    set(OPEN62541_SOURCES ${PROJECT_SOURCE_DIR}/third_party/open62541/open62541.c)
    set_source_files_properties(${OPEN62541_SOURCES} ${OPCUABRIDGE_SOURCES}
        PROPERTIES COMPILE_FLAGS ${OPEN62541_IGNORED_WARNINGS})
    list(APPEND SOURCES ${OPCUABRIDGE_SOURCES})
endif (BUILD_OPCUA)

# set the name of the executable
add_executable(aktualizr main.cc)
add_library(aktualizr_static_lib STATIC ${SOURCES} ${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp ${OPEN62541_SOURCES})
target_include_directories(aktualizr_static_lib PUBLIC ${LIBOSTREE_INCLUDE_DIRS})
target_link_libraries(aktualizr_static_lib socket_activation)

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
        ${LIBDPKG_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr gcov)
endif(BUILD_WITH_CODE_COVERAGE)

################ QA RULES
set(ALL_CHECKS main.cc ${SOURCES} ${HEADERS} ${OPCUABRIDGE_SOURCES} ${OPCUABRIDGE_HEADERS}
    ${DEB_SOURCES} ${OSTREE_SOURCES} ${P11_SOURCES})
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
