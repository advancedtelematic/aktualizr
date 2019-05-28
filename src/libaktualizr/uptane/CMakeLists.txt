set(SOURCES
    fetcher.cc
    iterator.cc
    managedsecondary.cc
    metawithkeys.cc
    partialverificationsecondary.cc
    role.cc
    root.cc
    secondaryconfig.cc
    secondaryfactory.cc
    tuf.cc
    uptanerepository.cc
    directorrepository.cc
    imagesrepository.cc
    virtualsecondary.cc)

set(HEADERS
    exceptions.h
    fetcher.h
    iterator.h
    managedsecondary.h
    partialverificationsecondary.h
    secondaryconfig.h
    secondaryfactory.h
    secondaryinterface.h
    tuf.h
    uptanerepository.h
    directorrepository.h
    imagesrepository.h
    virtualsecondary.h)


add_library(uptane OBJECT ${SOURCES})

if (BUILD_ISOTP)
    target_sources(uptane PRIVATE isotpsecondary.cc)
    target_include_directories(uptane PRIVATE ${PROJECT_SOURCE_DIR}/third_party/isotp-c/src)
endif (BUILD_ISOTP)

add_aktualizr_test(NAME tuf SOURCES tuf_test.cc PROJECT_WORKING_DIRECTORY)
add_aktualizr_test(NAME tuf_hash SOURCES tuf_hash_test.cc PROJECT_WORKING_DIRECTORY)

if(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)
    add_aktualizr_test(NAME uptane_ci SOURCES uptane_ci_test.cc PROJECT_WORKING_DIRECTORY NO_VALGRIND
                        ARGS ${SOTA_PACKED_CREDENTIALS} ${PROJECT_BINARY_DIR}/ostree_repo)
    set_tests_properties(test_uptane_ci PROPERTIES LABELS "credentials")
else(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)
    list(APPEND TEST_SOURCES uptane_ci_test.cc)
endif(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)


add_aktualizr_test(NAME uptane SOURCES uptane_test.cc PROJECT_WORKING_DIRECTORY)
set_tests_properties(test_uptane PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_delegation SOURCES uptane_delegation_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:aktualizr-repo>")
add_dependencies(t_uptane_delegation aktualizr-repo)
set_tests_properties(test_uptane_delegation PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_key SOURCES uptane_key_test.cc PROJECT_WORKING_DIRECTORY)
set_tests_properties(test_uptane_key PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_network SOURCES uptane_network_test.cc PROJECT_WORKING_DIRECTORY)
set_tests_properties(test_uptane_network PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_secondary SOURCES uptane_secondary_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME uptane_serial SOURCES uptane_serial_test.cc ARGS ${PROJECT_BINARY_DIR}
                   PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME uptane_init SOURCES uptane_init_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME director SOURCES director_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:aktualizr-repo>")

aktualizr_source_file_checks(${SOURCES} ${HEADERS} isotpsecondary.cc isotpsecondary.h ${TEST_SOURCES})
