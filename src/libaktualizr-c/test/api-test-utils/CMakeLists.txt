SET(TARGET_NAME api-test-utils)
SET(SOURCES api-test-utils.cc)
SET(HEADERS api-test-utils.h)

SET(CMAKE_SKIP_RPATH TRUE)

add_library(${TARGET_NAME} SHARED ${SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src/uptane_generator
  ${PROJECT_SOURCE_DIR}/src/virtual_secondary)
target_link_libraries(${TARGET_NAME}
  testutilities
  ${AKTUALIZR_EXTERNAL_LIBS}
  uptane_generator_lib
  virtual_secondary)

aktualizr_source_file_checks(${SOURCES} ${HEADERS})
