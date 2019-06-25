set(SOURCES managedsecondary.cc virtualsecondary.cc)

set(HEADERS managedsecondary.h virtualsecondary.h)

set(TARGET virtual_secondary)

add_library(${TARGET} STATIC
  ${SOURCES}
)

target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/src/virtual_secondary)
add_aktualizr_test(NAME virtual_secondary SOURCES virtual_secondary_test.cc PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${HEADERS} ${SOURCES} ${TEST_SOURCES})
