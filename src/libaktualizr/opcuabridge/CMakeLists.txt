set(SOURCES common.cc
            filelist.cc
            opcuabridgeclient.cc
            opcuabridgeserver.cc
            opcuabridgediscoveryclient.cc
            opcuabridgediscoveryserver.cc
            )

set(HEADERS boostarch.h
            common.h
            configuration.h
            currenttime.h
            ecuversionmanifest.h
            ecuversionmanifestsigned.h
            filedata.h
            filelist.h
            hash.h
            imageblock.h
            imagefile.h
            image.h
            imagerequest.h
            metadatafile.h
            metadatafiles.h
            opcuabridge.h
            opcuabridgeclient.h
            opcuabridgeconfig.h
            opcuabridgediscoveryclient.h
            opcuabridgediscoveryserver.h
            opcuabridgediscoverytypes.h
            opcuabridgeserver.h
            originalmanifest.h
            signature.h
            signed.h
            utility.h
            versionreport.h)

set_source_files_properties(${SOURCES} ${PROJECT_SOURCE_DIR}/third_party/open62541/open62541.c PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-float-equal -Wno-logical-op")
add_library(opcua_bridge OBJECT ${SOURCES} ${PROJECT_SOURCE_DIR}/third_party/open62541/open62541.c)
target_include_directories(opcua_bridge PRIVATE ${PROJECT_SOURCE_DIR}/third_party/open62541/)

include_directories(${PROJECT_SOURCE_DIR}/third_party/open62541/ ${PROJECT_SOURCE_DIR}/tests)
set_source_files_properties(opcuabridge_test_utils.cc opcuabridge_messaging_test.cc opcuabridge_server.cc
                            opcuabridge_secondary.cc opcuabridge_secondary_update_test.cc
                            opcuabridge_ostree_repo_sync_client.cc
                            opcuabridge_ostree_repo_sync_server.cc
                            PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-float-equal -Wno-logical-op")
add_executable(opcuabridge-server opcuabridge_server.cc opcuabridge_test_utils.cc)
target_link_libraries(opcuabridge-server aktualizr_static_lib ${TEST_LIBS})
add_dependencies(build_tests opcuabridge-server)

add_aktualizr_test(NAME opcuabridge_messaging
                    SOURCES opcuabridge_messaging_test.cc opcuabridge_test_utils.cc)

add_executable(opcuabridge-secondary opcuabridge_secondary.cc opcuabridge_test_utils.cc)
target_link_libraries(opcuabridge-secondary aktualizr_static_lib ${TEST_LIBS})
add_dependencies(build_tests opcuabridge-secondary)

add_aktualizr_test(NAME opcuabridge_secondary_update
                    SOURCES opcuabridge_secondary_update_test.cc opcuabridge_test_utils.cc)
set(TEST_SOURCES ${TEST_SOURCES} opcuabridge_messaging_test.cc opcuabridge_server.cc
                    opcuabridge_secondary.cc opcuabridge_secondary_update_test.cc
                    opcuabridge_test_utils.cc)

add_executable(opcuabridge-ostree-repo-sync-client opcuabridge_ostree_repo_sync_client.cc
                opcuabridge_test_utils.cc)
target_link_libraries(opcuabridge-ostree-repo-sync-client aktualizr_static_lib ${TEST_LIBS})
add_dependencies(build_tests opcuabridge-ostree-repo-sync-client)

add_executable(opcuabridge-ostree-repo-sync-server opcuabridge_ostree_repo_sync_server.cc
                opcuabridge_test_utils.cc)
target_link_libraries(opcuabridge-ostree-repo-sync-server aktualizr_static_lib ${TEST_LIBS})
add_dependencies(build_tests opcuabridge-ostree-repo-sync-server)

add_test(NAME opcuabridge-ostree-repo-sync-test
            COMMAND ${PROJECT_SOURCE_DIR}/tests/run_opcuabridge_ostree_repo_sync_test.sh
            ${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/tests)

add_test(NAME opcuabridge-ostree-repo-sync-valgrind-test
            COMMAND ${PROJECT_SOURCE_DIR}/tests/run_opcuabridge_ostree_repo_sync_test.sh
            ${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/tests ${RUN_VALGRIND})



aktualizr_source_file_checks(${SOURCES} ${HEADERS})
