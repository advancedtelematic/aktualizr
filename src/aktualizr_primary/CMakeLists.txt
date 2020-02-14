set(SOURCES main.cc secondary_config.cc secondary.cc)
set(HEADERS secondary_config.h secondary.h)

add_executable(aktualizr ${SOURCES})
target_link_libraries(aktualizr aktualizr_lib virtual_secondary aktualizr-posix)
target_include_directories(aktualizr PUBLIC ${PROJECT_SOURCE_DIR}/third_party)

install(TARGETS aktualizr RUNTIME DESTINATION bin COMPONENT aktualizr)

add_aktualizr_test(NAME primary_secondary_registration SOURCES primary_secondary_registration_test.cc secondary.cc secondary_config.cc PROJECT_WORKING_DIRECTORY ARGS ${PROJECT_BINARY_DIR}/uptane_repos LIBRARIES PUBLIC aktualizr-posix virtual_secondary uptane_generator_lib)
target_include_directories(t_primary_secondary_registration PUBLIC ${PROJECT_SOURCE_DIR}/src/libaktualizr-posix)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})

# vim: set tabstop=4 shiftwidth=4 expandtab:
