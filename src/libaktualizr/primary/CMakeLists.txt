set(SOURCES aktualizr.cc
            commands.cc
            events.cc
            eventsinterpreter.cc
            gatewaymanager.cc
            initializer.cc
            socketgateway.cc
            sotauptaneclient.cc)

set(HEADERS aktualizr.h
            commands.h
            events.h
            eventsinterpreter.h
            gateway.h
            gatewaymanager.h
            initializer.h
            socketgateway.h
            sotauptaneclient.h)


add_library(primary OBJECT ${SOURCES})

add_aktualizr_test(NAME commands SOURCES commands_test.cc)
add_aktualizr_test(NAME events SOURCES events_test.cc)
add_aktualizr_test(NAME socket_gateway SOURCES socketgateway_test.cc
                   ARGS ${PROJECT_SOURCE_DIR}/tests/fake_unix_socket/)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})
