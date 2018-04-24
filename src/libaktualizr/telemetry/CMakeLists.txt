set(SOURCES telemetryconfig.cc)

set(HEADERS telemetryconfig.h)

add_library(telemetry OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})