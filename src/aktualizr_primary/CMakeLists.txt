set(SOURCES main.cc secondary_config.h secondary_config.cc secondary.h secondary.cc)

add_executable(aktualizr ${SOURCES})
target_link_libraries(aktualizr aktualizr_lib virtual_secondary aktualizr-posix)
target_include_directories(aktualizr PUBLIC ${PROJECT_SOURCE_DIR}/third_party)

aktualizr_source_file_checks(${SOURCES})

install(TARGETS aktualizr RUNTIME DESTINATION bin COMPONENT aktualizr)

# vim: set tabstop=4 shiftwidth=4 expandtab:
