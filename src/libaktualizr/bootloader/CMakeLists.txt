set(HEADERS bootloader.h bootloader_config.h)
set(SOURCES bootloader.cc)

add_library(bootloader OBJECT ${SOURCES})
target_include_directories(bootloader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_aktualizr_test(NAME bootloader SOURCES bootloader_test.cc PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${HEADERS} ${SOURCES} ${TEST_SOURCES})
