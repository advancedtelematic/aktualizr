add_custom_command(OUTPUT sql_schemas.cc sql_schemas_target
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/embed_schemas.py ${PROJECT_SOURCE_DIR}/config/sql/ ${CMAKE_CURRENT_BINARY_DIR}/sql_schemas.cc libaktualizr
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

if(STORAGE_TYPE STREQUAL "sqlite")
  set(SOURCES sqlstorage.cc sqlstorage_base.cc)
  set(HEADERS sqlstorage.h sql_utils.h sqlstorage_base.h storage_exception.h)
elseif(STORAGE_TYPE STREQUAL "android")
  set(SOURCES androidstorage.cc)
  set(HEADERS androidstorage.h)
else()
  message(FATAL_ERROR "Unknown storage type: ${storage_type}")
endif()

set(HEADERS ${HEADERS} storage_config.h fsstorage_read.h invstorage.h)
set(SOURCES ${SOURCES} fsstorage_read.cc invstorage.cc)

aktualizr_source_file_checks(${SOURCES} ${HEADERS})

if(STORAGE_TYPE STREQUAL "sqlite")
  list(APPEND SOURCES sql_schemas.cc)

  add_aktualizr_test(NAME storage_atomic SOURCES storage_atomic_test.cc PROJECT_WORKING_DIRECTORY)
  add_aktualizr_test(NAME sql_utils SOURCES sql_utils_test.cc PROJECT_WORKING_DIRECTORY)
  add_aktualizr_test(NAME sqlstorage SOURCES sqlstorage_test.cc sql_schemas.cc
      ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test)
  list(REMOVE_ITEM TEST_SOURCES sql_schemas.cc)
  add_aktualizr_test(NAME storage SOURCES storage_common_test.cc PROJECT_WORKING_DIRECTORY)

  add_test(NAME test_schema_migration
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/schema_migration_test.sh ${PROJECT_SOURCE_DIR}/config/sql)
  set_tests_properties(test_schema_migration PROPERTIES LABELS "noptest")
endif(STORAGE_TYPE STREQUAL "sqlite")

add_library(storage OBJECT ${SOURCES})

aktualizr_source_file_checks(${TEST_SOURCES})
