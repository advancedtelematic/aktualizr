set(SOURCES initialize.cc
    ipsecondarydiscovery.cc
    ipuptanesecondary.cc
    legacysecondary.cc
    managedsecondary.cc
    partialverificationsecondary.cc
    role.cc
    root.cc
    tuf.cc
    uptanerepository.cc
    virtualsecondary.cc)

set(HEADERS exceptions.h
    ipsecondarydiscovery.h
    ipuptanesecondary.h
    legacysecondary.h
    managedsecondary.h
    partialverificationsecondary.h
    secondaryconfig.h
    secondaryfactory.h
    secondaryinterface.h
    tuf.h
    uptanerepository.h
    virtualsecondary.h)


add_library(uptane OBJECT ${SOURCES})
if (BUILD_OPCUA)
    target_sources(uptane PRIVATE opcuasecondary.cc)
    target_include_directories(uptane PRIVATE ${PROJECT_SOURCE_DIR}/src/libaktualizr/third_party/open62541/)
    set_source_files_properties(opcuasecondary.cc PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-float-equal -Wno-logical-op")
endif (BUILD_OPCUA)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} opcuasecondary.cc opcuasecondary.h)
