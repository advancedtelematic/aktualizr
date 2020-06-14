#set(HEADERS config.h)
set(SOURCES config.cc)

add_library(config OBJECT ${SOURCES})
# note: the Config object is composed with multiple sub-config objects that live
# close to the modules their refer too. To make the config module as
# self-contained as possible, the method definitions of these sub objects are
# also added to the module with CMake `target_sources(config PRIVATE ...)`
# declarations.

add_aktualizr_test(NAME config SOURCES config_test.cc ARGS ${PROJECT_BINARY_DIR} PROJECT_WORKING_DIRECTORY)

# config file test for collisions between import and FS->SQL migration paths
add_test(NAME config-import COMMAND ${PROJECT_SOURCE_DIR}/tests/run_import_clash_test.sh ${PROJECT_SOURCE_DIR}/config)

#aktualizr_source_file_checks(${SOURCES} ${HEADERS} config_test.cc)
aktualizr_source_file_checks(${SOURCES} config_test.cc)
