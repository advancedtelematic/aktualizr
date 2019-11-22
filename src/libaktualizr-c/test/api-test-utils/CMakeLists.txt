SET(TARGET_NAME api-test-utils)
SET(SOURCES api-test-utils.cc)
SET(HEADERS api-test-utils.h)

SET(CMAKE_SKIP_RPATH TRUE)

add_library(${TARGET_NAME} SHARED ${SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(${TARGET_NAME} testutilities)
add_dependencies(build_tests ${TARGET_NAME})

aktualizr_source_file_checks(${SOURCES} ${HEADERS})
