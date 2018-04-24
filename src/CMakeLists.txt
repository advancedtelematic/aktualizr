add_subdirectory("libaktualizr")

set(OPCUABRIDGE_SOURCES opcuabridge/common.cc
                        opcuabridge/filelist.cc
                        opcuabridge/opcuabridgeclient.cc
                        opcuabridge/opcuabridgeserver.cc
                        opcuabridge/opcuabridgediscoveryclient.cc
                        opcuabridge/opcuabridgediscoveryserver.cc
                        libaktualizr/uptane/opcuasecondary.cc)
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
                        libaktualizr/uptane/opcuasecondary.h)
if (BUILD_OPCUA)
    set(OPEN62541_SOURCES ${PROJECT_SOURCE_DIR}/third_party/open62541/open62541.c)
    set_source_files_properties(${OPEN62541_SOURCES} ${OPCUABRIDGE_SOURCES}
        PROPERTIES COMPILE_FLAGS ${OPEN62541_IGNORED_WARNINGS})
    add_library(opcua_bridge OBJECT ${OPCUABRIDGE_SOURCES} ${OPEN62541_SOURCES})
    target_sources(aktualizr_static_lib PRIVATE $<TARGET_OBJECTS:opcua_bridge>)
endif (BUILD_OPCUA)

add_executable(aktualizr aktualizr_primary/main.cc)
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

set(ALL_CHECKS aktualizr_primary/main.cc
    ${OPCUABRIDGE_SOURCES} ${OPCUABRIDGE_HEADERS})
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
