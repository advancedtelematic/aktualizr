
add_executable(aktualizr-get main.cc get.cc)
target_link_libraries(aktualizr-get aktualizr_lib)

install(TARGETS aktualizr-get RUNTIME DESTINATION bin COMPONENT aktualizr-get)

add_aktualizr_test(NAME aktualizr_get
                   SOURCES get.cc get_test.cc
                   PROJECT_WORKING_DIRECTORY)

# Check the --help option works.
add_test(NAME aktualizr-get-option-help
         COMMAND aktualizr-get --help)

# Report version.
add_test(NAME aktualizr-get-option-version
         COMMAND aktualizr-get --version)
set_tests_properties(aktualizr-get-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current aktualizr-get version is: ${AKTUALIZR_VERSION}")

aktualizr_source_file_checks(main.cc get.cc get.h get_test.cc)

# vim: set tabstop=4 shiftwidth=4 expandtab:
