set(SOURCES aktualizr.cc
            initializer.cc
            reportqueue.cc
            sotauptaneclient.cc)

set(HEADERS aktualizr.h
            initializer.h
            reportqueue.h
            sotauptaneclient.h)


add_library(primary OBJECT ${SOURCES})

add_aktualizr_test(NAME aktualizr SOURCES aktualizr_test.cc PROJECT_WORKING_DIRECTORY)
add_aktualizr_test(NAME reportqueue SOURCES reportqueue_test.cc PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})
