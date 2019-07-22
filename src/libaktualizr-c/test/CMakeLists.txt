SET(TARGET_NAME api-test)
SET(SOURCES api-test.c)

add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL ${SOURCES})
add_dependencies(build_tests ${TARGET_NAME})
target_link_libraries(${TARGET_NAME} aktualizr-c)

aktualizr_source_file_checks(${SOURCES})
