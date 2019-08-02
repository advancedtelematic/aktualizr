SET(TARGET_NAME aktualizr-c)
SET(SOURCES libaktualizr-c.cc)

add_library(${TARGET_NAME} SHARED ${SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${TARGET_NAME} PRIVATE aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

aktualizr_source_file_checks(${SOURCES})

add_subdirectory(test)
