set(AKTUALIZR_INFO_SRC main.cc aktualizr_info_config.cc)

set(AKTUALIZR_INFO_HEADERS aktualizr_info_config.h)

add_executable(aktualizr-info ${AKTUALIZR_INFO_SRC})
target_link_libraries(aktualizr-info aktualizr_lib)

install(TARGETS aktualizr-info
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

add_aktualizr_test(NAME aktualizr_info_config
                   SOURCES aktualizr_info_config.cc aktualizr_info_config_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME aktualizr_info SOURCES aktualizr_info_test.cc)

# Check the --help option works.
add_test(NAME aktualizr-info-option-help
         COMMAND aktualizr-info --help)

# Report version.
add_test(NAME aktualizr-info-option-version
         COMMAND aktualizr-info --version)
set_tests_properties(aktualizr-info-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current aktualizr-info version is: ${AKTUALIZR_VERSION}")

aktualizr_source_file_checks(${AKTUALIZR_INFO_SRC}
                             ${AKTUALIZR_INFO_HEADERS}
                             ${TEST_SOURCES})

# vim: set tabstop=4 shiftwidth=4 expandtab:
