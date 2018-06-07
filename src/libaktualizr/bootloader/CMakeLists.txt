set(HEADERS bootloader.h bootloader_config.h)
set(SOURCES bootloader.cc)

aktualizr_source_file_checks(${HEADERS} ${SOURCES})

add_library(bootloader OBJECT ${SOURCES})
target_include_directories(bootloader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

