set(HEADERS config.h)
set(SOURCES config.cc)
include(AddAktualizrTest)

add_library(config OBJECT ${SOURCES})

add_aktualizr_test(NAME config SOURCES config_test.cc ARGS ${PROJECT_BINARY_DIR} PROJECT_WORKING_DIRECTORY)

# config file test for collisions between import and FS->SQL migration paths
add_test(NAME config-import COMMAND ${PROJECT_SOURCE_DIR}/tests/run_import_clash_test.sh ${PROJECT_SOURCE_DIR}/config)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} config_test.cc)
