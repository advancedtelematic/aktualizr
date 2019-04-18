set(SOURCES aktualizr.cc
            initializer.cc
            reportqueue.cc
            sotauptaneclient.cc)

set(HEADERS aktualizr.h
            events.h
            initializer.h
            reportqueue.h
            results.h
            sotauptaneclient.h)


add_library(primary OBJECT ${SOURCES})

add_aktualizr_test(NAME aktualizr SOURCES aktualizr_test.cc PROJECT_WORKING_DIRECTORY ARGS ${PROJECT_BINARY_DIR}/uptane_repos)
add_dependencies(t_aktualizr uptane_repo_full_no_correlation_id)

add_aktualizr_test(NAME reportqueue SOURCES reportqueue_test.cc PROJECT_WORKING_DIRECTORY)
add_aktualizr_test(NAME emptytargets SOURCES empty_targets_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS "$<TARGET_FILE:aktualizr-repo>")

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})
