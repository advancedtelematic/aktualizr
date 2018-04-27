set(SOURCES aktualizr.cc
            commands.cc
            events.cc
            eventsinterpreter.cc
            gatewaymanager.cc
            socketgateway.cc
            sotauptaneclient.cc)

set(HEADERS aktualizr.h
            commands.h
            events.h
            eventsinterpreter.h
            gateway.h
            gatewaymanager.h
            socketgateway.h
            sotauptaneclient.h)


add_library(primary OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
