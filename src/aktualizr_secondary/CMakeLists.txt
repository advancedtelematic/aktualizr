set(AKTUALIZR_SECONDARY_SRC main.cc)

set(AKTUALIZR_SECONDARY_LIB_SRC
    aktualizr_secondary.cc
    aktualizr_secondary_config.cc
    aktualizr_secondary_common.cc
    aktualizr_secondary_metadata.cc
    socket_server.cc
    )

# do not link tests with libaktualizr
list(REMOVE_ITEM TEST_LIBS aktualizr_lib)

# TODO: a lot of these should be removed
add_library(aktualizr_secondary_lib SHARED
    ${AKTUALIZR_SECONDARY_LIB_SRC}
    $<TARGET_OBJECTS:aktualizr-posix>
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
    $<TARGET_OBJECTS:uptane>)
target_link_libraries(aktualizr_secondary_lib ${AKTUALIZR_EXTERNAL_LIBS})
target_include_directories(aktualizr_secondary_lib PUBLIC
    $<TARGET_PROPERTY:asn1_lib,INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src/libaktualizr-posix
    )

if (BUILD_ISOTP)
    target_sources(aktualizr_secondary_lib PRIVATE $<TARGET_OBJECTS:isotp_conn>)
endif (BUILD_ISOTP)

add_executable(aktualizr-secondary ${AKTUALIZR_SECONDARY_SRC})
target_link_libraries(aktualizr-secondary aktualizr_secondary_lib)
install(TARGETS aktualizr_secondary_lib LIBRARY DESTINATION lib COMPONENT aktualizr)

install(TARGETS aktualizr-secondary
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

set(ALL_AKTUALIZR_SECONDARY_HEADERS
    aktualizr_secondary.h
    aktualizr_secondary_interface.h
    aktualizr_secondary_config.h
    aktualizr_secondary_common.h
    aktualizr_secondary_metadata.h
    socket_server.h
    )

include(AddAktualizrTest)

# insert in front, so that the order matches the dependencies to the system libraries
list(INSERT TEST_LIBS 0 aktualizr_secondary_lib)

add_aktualizr_test(NAME aktualizr_secondary_config
                   SOURCES aktualizr_secondary_config_test.cc PROJECT_WORKING_DIRECTORY LIBRARIES aktualizr_secondary_lib)

add_aktualizr_test(NAME aktualizr_secondary_update
                   SOURCES update_test.cc
                   ARGS ${PROJECT_BINARY_DIR}/ostree_repo
                   PROJECT_WORKING_DIRECTORY LIBRARIES aktualizr_secondary_lib)

if(BUILD_OSTREE)
    add_aktualizr_test(NAME aktualizr_secondary_uptane_verification
                       SOURCES uptane_verification_test.cc
                       ARGS ${PROJECT_BINARY_DIR}/ostree_repo
                       LIBRARIES aktualizr_secondary_lib uptane_generator_lib $<TARGET_OBJECTS:campaign>
                       PROJECT_WORKING_DIRECTORY )

    set_target_properties(t_aktualizr_secondary_uptane_verification PROPERTIES LINK_FLAGS -Wl,--export-dynamic)
    target_link_libraries(t_aktualizr_secondary_uptane_verification aktualizr_secondary_lib uptane_generator_lib)

    add_aktualizr_test(NAME aktualizr_secondary_uptane
                       SOURCES uptane_test.cc
                       LIBRARIES aktualizr_secondary_lib uptane_generator_lib virtual_secondary $<TARGET_OBJECTS:campaign> $<TARGET_OBJECTS:primary> $<TARGET_OBJECTS:http>
                       ARGS ${PROJECT_BINARY_DIR}/ostree_repo PROJECT_WORKING_DIRECTORY)

    add_aktualizr_test(NAME aktualizr_secondary_uptane_verification_ostree
                       SOURCES uptane_verification_ostree_test.cc
                       ARGS ${PROJECT_BINARY_DIR}/ostree_repo
                       LIBRARIES aktualizr_secondary_lib uptane_generator_lib $<TARGET_OBJECTS:campaign>
                       PROJECT_WORKING_DIRECTORY)

    set_target_properties(t_aktualizr_secondary_uptane_verification_ostree PROPERTIES LINK_FLAGS -Wl,--export-dynamic)
    target_link_libraries(t_aktualizr_secondary_uptane_verification_ostree aktualizr_secondary_lib uptane_generator_lib)
else(BUILD_OSTREE)
    list(APPEND TEST_SOURCES uptane_verification_test.cc uptane_test.cc uptane_verification_ostree_test.cc)
endif(BUILD_OSTREE)

# test running the executable with command line option --help
add_test(NAME aktualizr_secondary_cmdline--help COMMAND aktualizr-secondary --help)
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
                                PASS_REGULAR_EXPRESSION "Aktualizr-secondary version")

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
