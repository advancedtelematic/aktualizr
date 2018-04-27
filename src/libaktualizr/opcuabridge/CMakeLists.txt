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

set_source_files_properties(${SOURCES} ${PROJECT_SOURCE_DIR}/src/libaktualizr/third_party/open62541/open62541.c PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-float-equal -Wno-logical-op")
add_library(opcua_bridge OBJECT ${SOURCES} ${PROJECT_SOURCE_DIR}/src/libaktualizr/third_party/open62541/open62541.c)
target_include_directories(opcua_bridge PRIVATE ${PROJECT_SOURCE_DIR}/src/libaktualizr/third_party/open62541/)
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
