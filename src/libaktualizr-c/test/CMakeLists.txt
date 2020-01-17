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

list(REMOVE_ITEM TEST_LIBS aktualizr_lib testutilities)
add_aktualizr_test(NAME c_api SOURCES api-test.c PROJECT_WORKING_DIRECTORY
    ARGS ${PROJECT_SOURCE_DIR}/tests/fake_http_server/fake_test_server.py ${REPO_PATH}
    LIBRARIES api-test-utils aktualizr-c)
add_dependencies(t_c_api api-test-resources-generation)
aktualizr_source_file_checks(api-test.c)

add_subdirectory(api-test-utils EXCLUDE_FROM_ALL)
