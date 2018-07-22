set(SOURCES aktualizr.cc
            commands.cc
            eventsinterpreter.cc
            initializer.cc
            reportqueue.cc
            sotauptaneclient.cc)

set(HEADERS aktualizr.h
            commands.h
            eventsinterpreter.h
            initializer.h
            reportqueue.h
            sotauptaneclient.h)


add_library(primary OBJECT ${SOURCES})

add_aktualizr_test(NAME commands SOURCES commands_test.cc)
add_aktualizr_test(NAME events SOURCES events_test.cc)
add_aktualizr_test(NAME eventsinterpreter SOURCES eventsinterpreter_test.cc)
add_aktualizr_test(NAME reportqueue SOURCES reportqueue_test.cc PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})
