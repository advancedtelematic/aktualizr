set(SOURCES base_config.cc config.cc)

target_sources(config PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/base_config.cc)

add_library(primary_config OBJECT config.cc)

add_aktualizr_test(NAME config SOURCES config_test.cc ARGS ${PROJECT_BINARY_DIR} PROJECT_WORKING_DIRECTORY)

# config file test for collisions between import and FS->SQL migration paths
add_test(NAME config-import COMMAND ${PROJECT_SOURCE_DIR}/tests/run_import_clash_test.sh ${PROJECT_SOURCE_DIR}/config)

aktualizr_source_file_checks(${SOURCES} config_test.cc)
