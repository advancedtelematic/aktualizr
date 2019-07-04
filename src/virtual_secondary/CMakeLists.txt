set(SOURCES managedsecondary.cc virtualsecondary.cc partialverificationsecondary.cc)

set(HEADERS managedsecondary.h virtualsecondary.h partialverificationsecondary.h)

set(TARGET virtual_secondary)

add_library(${TARGET} STATIC
  ${SOURCES}
)

target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_aktualizr_test(NAME virtual_secondary SOURCES virtual_secondary_test.cc PROJECT_WORKING_DIRECTORY)
target_link_libraries(t_virtual_secondary virtual_secondary)

aktualizr_source_file_checks(${HEADERS} ${SOURCES} ${TEST_SOURCES})
