set(SOURCES aktualizr.cc
            initializer.cc
            reportqueue.cc
            sotauptaneclient.cc)

set(HEADERS secondary_config.h
            aktualizr.h
            events.h
            initializer.h
            reportqueue.h
            results.h
            sotauptaneclient.h)


add_library(primary OBJECT ${SOURCES})

add_aktualizr_test(NAME aktualizr SOURCES aktualizr_test.cc PROJECT_WORKING_DIRECTORY ARGS ${PROJECT_BINARY_DIR}/uptane_repos LIBRARIES uptane_generator_lib)
add_dependencies(t_aktualizr uptane_repo_full_no_correlation_id)
target_link_libraries(t_aktualizr virtual_secondary)

if (BUILD_OSTREE)
    add_aktualizr_test(NAME aktualizr_fullostree SOURCES aktualizr_fullostree_test.cc PROJECT_WORKING_DIRECTORY ARGS $<TARGET_FILE:uptane-generator> ${PROJECT_BINARY_DIR}/ostree_repo)
    set_target_properties(t_aktualizr_fullostree PROPERTIES LINK_FLAGS -Wl,--export-dynamic)
<<<<<<< HEAD
    add_dependencies(t_aktualizr_fullostree ostree_mock aktualizr-repo make_ostree_sysroot)
    set_tests_properties(test_aktualizr_fullostree PROPERTIES ENVIRONMENT LD_PRELOAD=$<TARGET_FILE:ostree_mock>)
=======
    add_dependencies(t_aktualizr_fullostree ostree_mock uptane-generator make_ostree_sysroot)
    set_tests_properties(test_aktualizr_fullostree PROPERTIES
        ENVIRONMENT LD_PRELOAD=$<TARGET_FILE:ostree_mock>
        LABELS "noptest")
>>>>>>> 32c73eff... Rename aktualizr-repo to uptane-generator.
    target_link_libraries(t_aktualizr_fullostree virtual_secondary)
else (BUILD_OSTREE)
    aktualizr_source_file_checks(aktualizr_fullostree_test.cc)
endif (BUILD_OSTREE)

<<<<<<< HEAD
add_aktualizr_test(NAME reportqueue SOURCES reportqueue_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES PUBLIC aktualizr_repo_lib)
add_aktualizr_test(NAME emptytargets SOURCES empty_targets_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:aktualizr-repo>" LIBRARIES aktualizr_repo_lib)
target_link_libraries(t_emptytargets virtual_secondary)
=======
add_aktualizr_test(NAME reportqueue SOURCES reportqueue_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES PUBLIC uptane_generator_lib)
add_aktualizr_test(NAME empty_targets SOURCES empty_targets_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:uptane-generator>" LIBRARIES uptane_generator_lib)
target_link_libraries(t_empty_targets virtual_secondary)
add_aktualizr_test(NAME custom_url SOURCES custom_url_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:uptane-generator>" LIBRARIES uptane_generator_lib)
target_link_libraries(t_custom_url virtual_secondary)
add_aktualizr_test(NAME target_mismatch SOURCES target_mismatch_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:uptane-generator>" LIBRARIES uptane_generator_lib)
target_link_libraries(t_target_mismatch virtual_secondary)
>>>>>>> 32c73eff... Rename aktualizr-repo to uptane-generator.

add_aktualizr_test(NAME device_cred_prov SOURCES device_cred_prov_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES PUBLIC uptane_generator_lib)
set_tests_properties(test_device_cred_prov PROPERTIES LABELS "crypto")

add_aktualizr_test(NAME uptane_key SOURCES uptane_key_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES uptane_generator_lib)
set_tests_properties(test_uptane_key PROPERTIES LABELS "crypto")
target_include_directories(t_uptane_key PUBLIC ${PROJECT_SOURCE_DIR}/src/virtual_secondary)
target_link_libraries(t_uptane_key virtual_secondary)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})
