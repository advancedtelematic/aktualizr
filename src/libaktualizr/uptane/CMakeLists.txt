set(SOURCES
    fetcher.cc
    iterator.cc
    metawithkeys.cc
    role.cc
    root.cc
    tuf.cc
    uptanerepository.cc
    directorrepository.cc
    imagerepository.cc
    manifest.cc)

set(HEADERS
    exceptions.h
    fetcher.h
    iterator.h
    secondaryinterface.h
    tuf.h
    uptanerepository.h
    directorrepository.h
    imagerepository.h
    manifest.h)


add_library(uptane OBJECT ${SOURCES})

add_aktualizr_test(NAME tuf SOURCES tuf_test.cc PROJECT_WORKING_DIRECTORY)

if(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)
    add_aktualizr_test(NAME uptane_ci SOURCES uptane_ci_test.cc PROJECT_WORKING_DIRECTORY
                        ARGS ${SOTA_PACKED_CREDENTIALS} ${PROJECT_BINARY_DIR}/ostree_repo)
    set_tests_properties(test_uptane_ci PROPERTIES LABELS "credentials")
    target_link_libraries(t_uptane_ci virtual_secondary)
else(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)
    list(APPEND TEST_SOURCES uptane_ci_test.cc)
endif(BUILD_OSTREE AND SOTA_PACKED_CREDENTIALS)


add_aktualizr_test(NAME uptane SOURCES uptane_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES uptane_generator_lib)
set_property(SOURCE uptane_test.cc PROPERTY COMPILE_DEFINITIONS TEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}")
set_tests_properties(test_uptane PROPERTIES LABELS "crypto")
target_link_libraries(t_uptane virtual_secondary)

add_aktualizr_test(NAME uptane_delegation SOURCES uptane_delegation_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:uptane-generator>" LIBRARIES uptane_generator_lib)
add_dependencies(t_uptane_delegation uptane-generator)
target_link_libraries(t_uptane_delegation virtual_secondary)
set_tests_properties(test_uptane_delegation PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_network SOURCES uptane_network_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES uptane_generator_lib)
set_tests_properties(test_uptane_network PROPERTIES LABELS "crypto")
target_link_libraries(t_uptane_network virtual_secondary)

add_aktualizr_test(NAME uptane_serial SOURCES uptane_serial_test.cc
                   PROJECT_WORKING_DIRECTORY LIBRARIES uptane_generator_lib)
target_link_libraries(t_uptane_serial virtual_secondary)

add_aktualizr_test(NAME director SOURCES director_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:uptane-generator>")

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})
