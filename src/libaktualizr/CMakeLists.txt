set_source_files_properties(${PROJECT_SOURCE_DIR}/src/p11engine.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
add_subdirectory("utilities")
add_subdirectory("asn1")
add_subdirectory("uptane")
add_subdirectory("logging")
add_subdirectory("storage")
add_subdirectory("socket_activation")
add_subdirectory("secondary_ipc")
add_subdirectory("package_manager")
add_subdirectory("telemetry")

set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS -w)
add_library(jsoncpp OBJECT ${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp)

set(AKTUALIZR_SOURCES aktualizr.cc
            bootstrap.cc
            commands.cc
            config.cc
            events.cc
            eventsinterpreter.cc
            gatewaymanager.cc
            ipsecondarydiscovery.cc
            socketgateway.cc
            sotauptaneclient.cc
            timer.cc
            )

# set headers used for clang format
set(AKTUALIZR_HEADERS aktualizr.h
            bootstrap.h
            channel.h
            commands.h
            config.h
            events.h
            eventsinterpreter.h
            exceptions.h
            gateway.h
            gatewaymanager.h
            httpinterface.h
            ipsecondarydiscovery.h
            openssl_compat.h
            socketgateway.h
            sotauptaneclient.h
            sql_utils.h
            timer.h)

aktualizr_source_file_checks(${AKTUALIZR_SOURCES} ${AKTUALIZR_HEADERS})

add_library(aktualizr_static_lib STATIC
    ${AKTUALIZR_SOURCES}
    $<TARGET_OBJECTS:asn1>
    $<TARGET_OBJECTS:jsoncpp>
    $<TARGET_OBJECTS:package_manager>
    $<TARGET_OBJECTS:utilities>
    $<TARGET_OBJECTS:aktualizr_secondary_ipc>
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:uptane>
    $<TARGET_OBJECTS:socket_activation>
    $<TARGET_OBJECTS:telemetry>)

target_include_directories(aktualizr_static_lib PUBLIC
    $<TARGET_PROPERTY:package_manager,INCLUDE_DIRECTORIES>)

