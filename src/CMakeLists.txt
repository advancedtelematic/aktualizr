set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS -w)
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/p11engine.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

add_subdirectory("utilities")
add_subdirectory("asn1")
add_subdirectory("uptane")
add_subdirectory("socket_activation")
add_subdirectory("secondary_ipc")
add_subdirectory("package_manager")
add_subdirectory("telemetry")

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
            bootstrap.h
            channel.h
            commands.h
            config.h
            events.h
            eventsinterpreter.h
            exceptions.h
            fsstorage.h
            gateway.h
            gatewaymanager.h
            httpinterface.h
            invstorage.h
            ipsecondarydiscovery.h
            logging.h
            openssl_compat.h
            socketgateway.h
            sotauptaneclient.h
            sqlstorage.h
            sql_utils.h
            timer.h)

set(STORAGE_SOURCES fsstorage.cc sqlstorage.cc invstorage.cc)
add_library(storage OBJECT ${STORAGE_SOURCES})

add_library(jsoncpp OBJECT ${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp)

add_library(aktualizr_static_lib STATIC
    ${AKTUALIZR_SOURCES}
    $<TARGET_OBJECTS:asn1>
    $<TARGET_OBJECTS:jsoncpp>
    $<TARGET_OBJECTS:package_manager>
    $<TARGET_OBJECTS:utilities>
    $<TARGET_OBJECTS:aktualizr_secondary_ipc>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:uptane>
    $<TARGET_OBJECTS:socket_activation>
    $<TARGET_OBJECTS:telemetry>)

target_include_directories(aktualizr_static_lib PUBLIC
    $<TARGET_PROPERTY:package_manager,INCLUDE_DIRECTORIES>)

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
    target_sources(aktualizr_static_lib PRIVATE $<TARGET_OBJECTS:opcua_bridge>)
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


add_subdirectory("cert_provider")
add_subdirectory("implicit_writer")
add_subdirectory("external_secondaries")


if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr gcov)
endif(BUILD_WITH_CODE_COVERAGE)

################ QA RULES

set(ALL_CHECKS main.cc
    ${AKTUALIZR_SOURCES}
    ${P11_SOURCES}
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
