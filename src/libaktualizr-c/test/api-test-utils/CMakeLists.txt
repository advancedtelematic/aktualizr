SET(TARGET_NAME api-test-utils)
SET(SOURCES api-test-utils.cc)
SET(HEADERS api-test-utils.h)

SET(CMAKE_SKIP_RPATH TRUE)

add_library(${TARGET_NAME} SHARED ${SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(${TARGET_NAME} testutilities)
add_dependencies(build_tests ${TARGET_NAME})

aktualizr_source_file_checks(${SOURCES} ${HEADERS})



add_executable(uptane_generator_utils uptane-generator-utils.cc)
target_include_directories(uptane_generator_utils PUBLIC ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(uptane_generator_utils
  aktualizr_static_lib
  testutilities
  uptane_generator_lib
  ${TEST_LIBS})
add_dependencies(build_tests uptane_generator_utils)

aktualizr_source_file_checks(uptane-generator-utils.cc)

add_test(NAME t_uptane_generator_setup COMMAND uptane_generator_utils
  ${PROJECT_BINARY_DIR}/src/uptane_generator/uptane-generator
  ${PROJECT_BINARY_DIR}/c-api-test-repo
  ${PROJECT_SOURCE_DIR}/tests/test_data/firmware.txt)
set_tests_properties(t_uptane_generator_setup PROPERTIES FIXTURES_SETUP c_api_test)

add_test(NAME t_uptane_generator_cleanup COMMAND uptane_generator_utils
  ${PROJECT_BINARY_DIR}/c-api-test-repo)
set_tests_properties(t_uptane_generator_cleanup PROPERTIES FIXTURES_CLEANUP c_api_test)
