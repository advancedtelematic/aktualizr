set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS -w)
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/p11engine.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)


# set source files excluding main
set(SOURCES aktualizr.cc
            asn1-cer.cc
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
            keymanager.cc
            logging.cc
            socketgateway.cc
            sotauptaneclient.cc
            sqlstorage.cc
            timer.cc
            types.cc
            utils.cc
            uptane/initialize.cc
            uptane/legacysecondary.cc
            uptane/managedsecondary.cc
            uptane/role.cc
            uptane/root.cc
            uptane/tuf.cc
            uptane/tufrepository.cc
            uptane/uptanerepository.cc
            uptane/virtualsecondary.cc)

# set headers used for clang format
set(HEADERS aktualizr.h
            bootstrap.h
            channel.h
            commands.h
            config.h
            crypto.h
            deb.h
            events.h
            eventsinterpreter.h
            exceptions.h
            fsstorage.h
            gatewaymanager.h
            httpclient.h
            httpinterface.h
            invstorage.h
            keymanager.h
            logging.h
            openssl_compat.h
            ostree.h
            packagemanagerfactory.h
            packagemanagerfake.h
            packagemanagerinterface.h
            socketgateway.h
            sotauptaneclient.h
            sqlstorage.h
            timer.h
            types.h
            utils.h
            uptane/exceptions.h
            uptane/legacysecondary.h
            uptane/managedsecondary.h
            uptane/secondaryconfig.h
            uptane/secondaryfactory.h
            uptane/secondaryinterface.h
            uptane/tuf.h
            uptane/tufrepository.h
            uptane/uptanerepository.h
            uptane/virtualsecondary.h)

if(BUILD_OSTREE)
    list(APPEND SOURCES ostree.cc)
endif(BUILD_OSTREE)

if(BUILD_DEB)
    list(APPEND SOURCES deb.cc)
endif(BUILD_DEB)


if(BUILD_P11)
    list(APPEND SOURCES p11engine.cc)
    list(APPEND HEADERS p11engine.h)

    if(TEST_PKCS11_MODULE_PATH)
        add_definitions(-DTEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}" -DTEST_PKCS11_ENGINE_PATH="${TEST_PKCS11_ENGINE_PATH}")
    endif(TEST_PKCS11_MODULE_PATH)
endif(BUILD_P11)

if(BUILD_OPCUA)
    set(OPEN62541_SOURCES ${PROJECT_SOURCE_DIR}/third_party/open62541/open62541.c)
    set(OPCUABRIDGE_SOURCES opcuabridge/common.cc)
    set(OPCUABRIDGE_HEADERS opcuabridge/boostarch.h
                            opcuabridge/common.h
                            opcuabridge/currenttime.h
                            opcuabridge/ecuversionmanifest.h
                            opcuabridge/ecuversionmanifestsigned.h
                            opcuabridge/hash.h
                            opcuabridge/imageblock.h
                            opcuabridge/imagefile.h
                            opcuabridge/image.h
                            opcuabridge/imagerequest.h
                            opcuabridge/metadatafile.h
                            opcuabridge/metadatafiles.h
                            opcuabridge/opcuabridge.h
                            opcuabridge/signature.h
                            opcuabridge/signed.h
                            opcuabridge/utility.h
                            opcuabridge/versionreport.h)
    set_source_files_properties(opcuabridge_messaging.cc ${OPEN62541_SOURCES} ${OPCUABRIDGE_SOURCES}
                                PROPERTIES COMPILE_FLAGS ${OPEN62541_IGNORED_WARNINGS})
endif(BUILD_OPCUA)

# set the name of the executable
add_executable(aktualizr main.cc)
add_library(aktualizr_static_lib STATIC ${SOURCES} ${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp
                                        ${OPEN62541_SOURCES} ${OPCUABRIDGE_SOURCES})
target_include_directories(aktualizr_static_lib PUBLIC ${LIBOSTREE_INCLUDE_DIRS})

target_link_libraries(aktualizr aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${sodium_LIBRARY_RELEASE}
        ${LIBOSTREE_LIBRARIES}
        ${LIBDBUS_LIBRARIES}
        ${LIBP11_LIBRARIES}
        ${SQLITE3_LIBRARIES}
        ${LibArchive_LIBRARIES}
        ${LIBDPKG_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr gcov)
endif(BUILD_WITH_CODE_COVERAGE)

################ QA RULES
aktualizr_source_file_checks(main.cc ${SOURCES} ${HEADERS} ${OPCUABRIDGE_SOURCES} ${OPCUABRIDGE_HEADERS})

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
