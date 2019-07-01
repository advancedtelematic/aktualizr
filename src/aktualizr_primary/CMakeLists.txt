set(TARGET aktualizr)
set(SOURCES main.cc secondary_config.h secondary_config.cc secondary.h secondary.cc)

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} aktualizr_static_lib aktualizr-posix virtual_secondary ${AKTUALIZR_EXTERNAL_LIBS})
target_include_directories(${TARGET} PUBLIC
  ${PROJECT_SOURCE_DIR}/src/libaktualizr-posix
  ${PROJECT_SOURCE_DIR}/third_party)

aktualizr_source_file_checks(${SOURCES})

install(TARGETS aktualizr RUNTIME DESTINATION bin COMPONENT aktualizr)

# vim: set tabstop=4 shiftwidth=4 expandtab:
