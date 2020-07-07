set(SOURCES telemetryconfig.cc)

target_sources(config PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/telemetryconfig.cc)
aktualizr_source_file_checks(${SOURCES})
