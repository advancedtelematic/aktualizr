set(SOURCES aktualizr.cc
            initializer.cc
            reportqueue.cc
            sotauptaneclient.cc)

set(HEADERS aktualizr.h
            initializer.h
            reportqueue.h
            sotauptaneclient.h)


add_library(primary OBJECT ${SOURCES})

# TODO: remove?:
#add_aktualizr_test(NAME eventsinterpreter SOURCES eventsinterpreter_test.cc)

add_aktualizr_test(NAME events SOURCES events_test.cc)
add_aktualizr_test(NAME reportqueue SOURCES reportqueue_test.cc PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES} eventsinterpreter_test.cc)
