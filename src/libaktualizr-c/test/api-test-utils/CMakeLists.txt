SET(TARGET_NAME api-test-utils)
SET(SOURCES api-test-utils.cc)
SET(HEADERS api-test-utils.h)

SET(CMAKE_SKIP_RPATH TRUE)

add_library(${TARGET_NAME} SHARED ${SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${TARGET_NAME} testutilities ${AKTUALIZR_EXTERNAL_LIBS} )

aktualizr_source_file_checks(${SOURCES} ${HEADERS})
