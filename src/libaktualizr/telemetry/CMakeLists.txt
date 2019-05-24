set(SOURCES telemetryconfig.cc)

set(HEADERS telemetryconfig.h)

target_sources(config PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/telemetryconfig.cc)
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
