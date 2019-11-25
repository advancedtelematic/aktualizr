SET(TARGET_NAME api-test)
SET(SOURCES api-test.c)

SET(CMAKE_SKIP_RPATH TRUE)

SET(REPO_PATH ${PROJECT_BINARY_DIR}/uptane_repos/c-api-test-repo)

if(CMAKE_CROSSCOMPILING)
    find_program(UPTANE_GENERATOR NAMES uptane-generator)
else()
    set(UPTANE_GENERATOR $<TARGET_FILE:uptane-generator>)
endif()

add_custom_target(api-test-resources-generation
    COMMAND ${PROJECT_SOURCE_DIR}/tests/uptane_repo_generation/generate_repo.sh
    ${UPTANE_GENERATOR}
    ${REPO_PATH}
    --add_campaigns)
add_dependencies(api-test-resources-generation uptane-generator)

add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL ${SOURCES})
add_dependencies(build_tests ${TARGET_NAME})
target_link_libraries(${TARGET_NAME} aktualizr-c api-test-utils)
add_dependencies(${TARGET_NAME} api-test-resources-generation)

aktualizr_source_file_checks(${SOURCES})

add_test(NAME t_c_api_test COMMAND ${TARGET_NAME}
    ${PROJECT_SOURCE_DIR}/tests/fake_http_server/fake_test_server.py
    ${REPO_PATH}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(t_c_api_test PROPERTIES
    ENVIRONMENT LD_PRELOAD=${PROJECT_BINARY_DIR}/src/libaktualizr-c/test/api-test-utils/libapi-test-utils.so:${PROJECT_BINARY_DIR}/src/libaktualizr-c/libaktualizr-c.so)

add_subdirectory(api-test-utils EXCLUDE_FROM_ALL)
