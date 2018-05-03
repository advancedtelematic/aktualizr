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
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
