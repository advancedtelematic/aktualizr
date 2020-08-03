set(SOURCES managedsecondary.cc virtualsecondary.cc)

set(HEADERS managedsecondary.h virtualsecondary.h)

set(TARGET virtual_secondary)

add_library(${TARGET} STATIC
  ${SOURCES}
)

target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/src/virtual_secondary)

add_aktualizr_test(NAME virtual_secondary SOURCES virtual_secondary_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES uptane_generator_lib)
target_link_libraries(t_virtual_secondary virtual_secondary)

aktualizr_source_file_checks(${HEADERS} ${SOURCES} ${TEST_SOURCES})
