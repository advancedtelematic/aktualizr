set(AKTUALIZR_SECONDARY_SRC main.cc)

set(AKTUALIZR_SECONDARY_LIB_SRC
    update_agent_file.cc
    aktualizr_secondary.cc
    aktualizr_secondary_file.cc
    aktualizr_secondary_config.cc
    aktualizr_secondary_metadata.cc
    secondary_tcp_server.cc
    msg_handler.cc
    )

# do not link tests with libaktualizr
list(REMOVE_ITEM TEST_LIBS aktualizr_lib)

# TODO(OTA-4544): a lot of these should be removed
add_library(aktualizr_secondary_lib SHARED
    ${AKTUALIZR_SECONDARY_LIB_SRC}
    $<TARGET_OBJECTS:asn1>
    $<TARGET_OBJECTS:asn1_lib>
    $<TARGET_OBJECTS:bootstrap>
    $<TARGET_OBJECTS:config>
    $<TARGET_OBJECTS:bootloader>
    $<TARGET_OBJECTS:crypto>
    $<TARGET_OBJECTS:jsoncpp>
    $<TARGET_OBJECTS:package_manager>
    $<TARGET_OBJECTS:utilities>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:uptane>
    $<TARGET_OBJECTS:campaign>)
set_target_properties(aktualizr_secondary_lib PROPERTIES LIBRARY_OUTPUT_NAME aktualizr_secondary)
target_link_libraries(aktualizr_secondary_lib aktualizr-posix ${AKTUALIZR_EXTERNAL_LIBS})

add_executable(aktualizr-secondary ${AKTUALIZR_SECONDARY_SRC})
target_link_libraries(aktualizr-secondary aktualizr_secondary_lib)
install(TARGETS aktualizr_secondary_lib LIBRARY DESTINATION lib COMPONENT aktualizr)

install(TARGETS aktualizr-secondary
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

set(ALL_AKTUALIZR_SECONDARY_HEADERS
    update_agent.h
    update_agent_file.h
    aktualizr_secondary.h
    aktualizr_secondary_file.h
    aktualizr_secondary_config.h
    aktualizr_secondary_metadata.h
    secondary_tcp_server.h
    msg_handler.h
    )

# insert in front, so that the order matches the dependencies to the system libraries
list(INSERT TEST_LIBS 0 aktualizr_secondary_lib)

add_aktualizr_test(NAME aktualizr_secondary_config
    SOURCES aktualizr_secondary_config_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES aktualizr_secondary_lib)

add_aktualizr_test(NAME secondary_rpc SOURCES $<TARGET_OBJECTS:primary> $<TARGET_OBJECTS:http> secondary_rpc_test.cc PROJECT_WORKING_DIRECTORY)
list(REMOVE_ITEM TEST_SOURCES $<TARGET_OBJECTS:primary> $<TARGET_OBJECTS:http>)

if(BUILD_OSTREE)
    target_sources(aktualizr_secondary_lib PRIVATE update_agent_ostree.cc aktualizr_secondary_ostree.cc)
    list(APPEND  AKTUALIZR_SECONDARY_LIB_SRC update_agent_ostree.cc aktualizr_secondary_ostree.cc)
    list(APPEND ALL_AKTUALIZR_SECONDARY_HEADERS update_agent_ostree.h aktualizr_secondary_ostree.h)

    add_aktualizr_test(NAME aktualizr_secondary_ostree
                       SOURCES aktualizr_secondary_ostree_test.cc
                       ARGS ${PROJECT_BINARY_DIR}/ostree_repo
                       LIBRARIES aktualizr_secondary_lib uptane_generator_lib
                       PROJECT_WORKING_DIRECTORY)

    set_target_properties(t_aktualizr_secondary_ostree PROPERTIES LINK_FLAGS -Wl,--export-dynamic)
else(BUILD_OSTREE)
    list(APPEND TEST_SOURCES aktualizr_secondary_ostree_test.cc update_agent_ostree.cc aktualizr_secondary_ostree.cc)
    list(APPEND ALL_AKTUALIZR_SECONDARY_HEADERS update_agent_ostree.h aktualizr_secondary_ostree.h)
endif(BUILD_OSTREE)

add_aktualizr_test(NAME aktualizr_secondary
                   SOURCES aktualizr_secondary_test.cc
                   LIBRARIES aktualizr_secondary_lib uptane_generator_lib)

# Check the --help option works.
add_test(NAME aktualizr-secondary-option-help
         COMMAND aktualizr-secondary --help)

# Report version.
add_test(NAME aktualizr-secondary-option-version
         COMMAND aktualizr-secondary --version)
set_tests_properties(aktualizr-secondary-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current aktualizr-secondary version is: ${AKTUALIZR_VERSION}")

# test running the executable with command line option --something
add_test(NAME aktualizr_secondary_cmdline--something
         COMMAND aktualizr-secondary --something -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml)
# test the return code when running the executable with non-existent configuration file
add_test(NAME aktualizr_secondary_no_config_check_code
         COMMAND aktualizr-secondary -c non-existent-config.toml)

# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass
set_tests_properties(aktualizr_secondary_cmdline--something
                     aktualizr_secondary_no_config_check_code
                     PROPERTIES WILL_FAIL TRUE)

# Check for expected output messages given specific misconfigurations
# test the error message when running the executable with non-existent configuration file
add_test(NAME aktualizr_secondary_no_config_check_message
         COMMAND aktualizr-secondary -c non-existent-config.toml)
set_tests_properties(aktualizr_secondary_no_config_check_message
                     PROPERTIES PASS_REGULAR_EXPRESSION "Config directory non-existent-config.toml does not exist.")

add_test(NAME aktualizr_secondary_help_with_other_options
         COMMAND aktualizr-secondary --help -c someconfig.toml)
set_tests_properties(aktualizr_secondary_help_with_other_options
                     PROPERTIES PASS_REGULAR_EXPRESSION "aktualizr-secondary command line options")

add_test(NAME aktualizr_secondary_help_with_nonexistent_options
         COMMAND aktualizr-secondary --help -somebadoption)
set_tests_properties(aktualizr_secondary_help_with_nonexistent_options
                     PROPERTIES PASS_REGULAR_EXPRESSION "aktualizr-secondary command line options")

# Check aktualizr-secondary verbose config parsing output with debug loglevel.
add_test(NAME aktualizr_secondary_log_debug
         COMMAND aktualizr-secondary -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml --loglevel=0)
set_tests_properties(aktualizr_secondary_log_debug
                     PROPERTIES PASS_REGULAR_EXPRESSION "Final configuration that will be used")
# Check aktualizr-secondary silent config parsing output with default loglevel.
# Note that the extra PASS is necessary to ignore the return code.
add_test(NAME aktualizr_secondary_log_default
         COMMAND aktualizr-secondary -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml)
set_tests_properties(aktualizr_secondary_log_default
                     PROPERTIES FAIL_REGULAR_EXPRESSION "Final configuration that will be used"
                                PASS_REGULAR_EXPRESSION "aktualizr-secondary version")

# Check aktualizr-secondary invalid logging levels.
add_test(NAME aktualizr_secondary_log_invalid
         COMMAND aktualizr-secondary -c ${PROJECT_SOURCE_DIR}/tests/config/aktualizr_secondary.toml --loglevel=6)
set_tests_properties(aktualizr_secondary_log_invalid
                     PROPERTIES PASS_REGULAR_EXPRESSION "Invalid log level")
add_test(NAME aktualizr_secondary_log_negative
         COMMAND aktualizr-secondary -c ${PROJECT_SOURCE_DIR}/tests/config/aktualizr_secondary.toml --loglevel=-1)
set_tests_properties(aktualizr_secondary_log_negative
                     PROPERTIES PASS_REGULAR_EXPRESSION "Invalid log level")

# these tests pose problem on ptest and are not really worth running there
set_tests_properties(aktualizr_secondary_help_with_other_options
    aktualizr_secondary_help_with_nonexistent_options
    aktualizr_secondary_log_debug
    aktualizr_secondary_log_default
    aktualizr_secondary_log_invalid
    aktualizr_secondary_log_negative
    PROPERTIES LABELS "noptest")

aktualizr_source_file_checks(${AKTUALIZR_SECONDARY_SRC}
                             ${AKTUALIZR_SECONDARY_LIB_SRC}
                             ${ALL_AKTUALIZR_SECONDARY_HEADERS}
                             ${TEST_SOURCES})

# vim: set tabstop=4 shiftwidth=4 expandtab:
