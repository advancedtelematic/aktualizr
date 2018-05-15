set(SOURCES fetcher.cc
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
    fetcher.h
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
    target_include_directories(uptane PRIVATE ${PROJECT_SOURCE_DIR}/third_party/open62541)
    set_source_files_properties(opcuasecondary.cc PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-float-equal -Wno-logical-op")
endif (BUILD_OPCUA)

add_aktualizr_test(NAME discovery_secondary SOURCES ipsecondary_discovery_test.cc PROJECT_WORKING_DIRECTORY)
add_aktualizr_test(NAME tuf SOURCES tuf_test.cc PROJECT_WORKING_DIRECTORY)

if(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)
    add_aktualizr_test(NAME uptane_ci SOURCES uptane_ci_test.cc PROJECT_WORKING_DIRECTORY NO_VALGRIND
                        ARGS ${SOTA_PACKED_CREDENTIALS} ${PROJECT_BINARY_DIR}/ostree_repo)
    set_tests_properties(test_uptane_ci PROPERTIES LABELS "credentials")
else(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)
    list(APPEND TEST_SOURCES uptane_ci_test.cc)
endif(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)


add_aktualizr_test(NAME uptane SOURCES uptane_test.cc PROJECT_WORKING_DIRECTORY
                       ARGS ${PROJECT_BINARY_DIR}/ostree_repo)
set_tests_properties(test_uptane PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_implicit SOURCES uptane_implicit_test.cc PROJECT_WORKING_DIRECTORY)
set_tests_properties(test_uptane_implicit PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_key SOURCES uptane_key_test.cc PROJECT_WORKING_DIRECTORY)
set_tests_properties(test_uptane_key PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_network SOURCES uptane_network_test.cc PROJECT_WORKING_DIRECTORY)
set_tests_properties(test_uptane_network PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_secondary SOURCES uptane_secondary_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME uptane_serial SOURCES uptane_serial_test.cc ARGS ${PROJECT_BINARY_DIR}
                   PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} opcuasecondary.cc opcuasecondary.h ${TEST_SOURCES})
