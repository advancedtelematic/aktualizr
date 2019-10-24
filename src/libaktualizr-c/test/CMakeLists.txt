SET(TARGET_NAME api-test)
SET(SOURCES api-test.c)

SET(CMAKE_SKIP_RPATH TRUE)

add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL ${SOURCES})
add_dependencies(build_tests ${TARGET_NAME})
target_link_libraries(${TARGET_NAME} aktualizr-c api-test-utils)

aktualizr_source_file_checks(${SOURCES})

add_test(NAME t_c_api_test COMMAND ${TARGET_NAME}
    ${PROJECT_SOURCE_DIR}/tests/fake_http_server/fake_test_server.py
	${PROJECT_BINARY_DIR}/c-api-test-repo
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(t_c_api_test PROPERTIES FIXTURES_REQUIRED c_api_test)
set_tests_properties(t_c_api_test PROPERTIES
    ENVIRONMENT LD_PRELOAD=${PROJECT_BINARY_DIR}/src/libaktualizr-c/test/api-test-utils/libapi-test-utils.so:${PROJECT_BINARY_DIR}/src/libaktualizr-c/libaktualizr-c.so)

add_subdirectory(api-test-utils EXCLUDE_FROM_ALL)
