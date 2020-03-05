option(INSTALL_GTEST "" OFF)
if(NOT GTEST_ROOT )
    set(GTEST_ROOT ${PROJECT_SOURCE_DIR}/third_party/googletest)
endif()

if(CMAKE_CROSSCOMPILING)
    # we cannot call the tool we build in this context, it has to come from somewhere else
    find_program(UPTANE_GENERATOR NAMES uptane-generator)
else()
    set(UPTANE_GENERATOR $<TARGET_FILE:uptane-generator>)
endif()

# Temporarily remove flags not supported by gtest.
remove_definitions(-Wswitch-default)
add_subdirectory(${GTEST_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gtest EXCLUDE_FROM_ALL)
add_definitions(-Wswitch-default)
add_subdirectory(uptane_repo_generation)

add_dependencies(build_tests aktualizr)
if(BUILD_SOTA_TOOLS)
    add_dependencies(build_tests garage-push)
    add_dependencies(build_tests garage-check)
endif(BUILD_SOTA_TOOLS)
add_dependencies(build_tests aktualizr-secondary)
add_dependencies(build_tests aktualizr-info)
add_dependencies(build_tests uptane-generator)

set(CTEST_EXTRA_ARGS -T Test --no-compress-output)

# build list of tests to run
foreach(label ${TESTSUITE_ONLY})
    set(CTEST_EXTRA_ARGS ${CTEST_EXTRA_ARGS} -L ${label})
endforeach()

foreach(label ${TESTSUITE_EXCLUDE})
    set(CTEST_EXTRA_ARGS ${CTEST_EXTRA_ARGS} -LE ${label})
endforeach()

add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=1 ${CMAKE_CTEST_COMMAND} ${CTEST_EXTRA_ARGS}
                  DEPENDS build_tests
                  USES_TERMINAL
                  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
                  )


# List of source files to run static analysis on. Automatically appended to by
# add_aktualizr_test, but anything that doesn't use that must be manually added
# to this list.
set(TEST_SOURCES httpfake.h metafake.h test_utils.cc test_utils.h uptane_vector_tests.cc)


include(CMakeParseArguments)

add_library(testutilities STATIC test_utils.cc)

if(BUILD_OSTREE)
    add_library(ostree_mock SHARED ostree_mock.c)
    aktualizr_source_file_checks(ostree_mock.c)
    target_include_directories(ostree_mock PUBLIC ${LIBOSTREE_INCLUDE_DIRS})
    add_dependencies(build_tests ostree_mock)
endif(BUILD_OSTREE)

# Setup coverage
if(BUILD_WITH_CODE_COVERAGE)
    add_definitions(${COVERAGE_COMPILER_FLAGS})
    setup_target_for_coverage_lcov(NAME coverage EXECUTABLE ${CMAKE_CTEST_COMMAND} ${CTEST_EXTRA_ARGS} USES_TERMINAL)
    add_dependencies(coverage build_tests)
endif(BUILD_WITH_CODE_COVERAGE)

if(TESTSUITE_VALGRIND)
    # This test is only checking that valgrind finds memory leaks
    add_aktualizr_test(NAME leak SOURCES leak_test.cc)
    set_tests_properties(test_leak PROPERTIES WILL_FAIL TRUE)
endif()

if(FAULT_INJECTION)
    add_aktualizr_test(NAME fiuinfo SOURCES fiuinfo_test.cc NO_VALGRIND LAUNCH_CMD ${PROJECT_SOURCE_DIR}/scripts/fiu run -c "enable name=fail,failinfo=fiurun_failure" -- ARGS ${PROJECT_SOURCE_DIR}/scripts/fiu)
endif()


add_executable(aktualizr_uptane_vector_tests uptane_vector_tests.cc)
target_link_libraries(aktualizr_uptane_vector_tests ${TEST_LIBS})
add_dependencies(build_tests aktualizr_uptane_vector_tests)

if(TESTSUITE_VALGRIND)
    set(VECTOR_TESTS_ARGS -v ${CMAKE_CURRENT_BINARY_DIR}/../run-valgrind)
endif(TESTSUITE_VALGRIND)
add_test(NAME test_uptane_vectors COMMAND ${PROJECT_SOURCE_DIR}/tests/run_vector_tests.sh
    -s ${PROJECT_SOURCE_DIR}/tests ${VECTOR_TESTS_ARGS} -- ${GOOGLE_TEST_OUTPUT})
set_tests_properties(test_uptane_vectors PROPERTIES LABELS "noptest uptane_vectors")

if(SOTA_PACKED_CREDENTIALS)
    add_test(NAME shared_cred_prov_test COMMAND ${PROJECT_SOURCE_DIR}/tests/shared_cred_prov_test.py
             --build-dir ${PROJECT_BINARY_DIR}
             --credentials ${SOTA_PACKED_CREDENTIALS})
    set_tests_properties(shared_cred_prov_test PROPERTIES LABELS "credentials")

    add_test(NAME device_cred_prov_test COMMAND ${PROJECT_SOURCE_DIR}/tests/device_cred_prov_test.py
             --build-dir ${PROJECT_BINARY_DIR}
             --credentials ${SOTA_PACKED_CREDENTIALS})
    set_tests_properties(device_cred_prov_test PROPERTIES LABELS "credentials")

    if(BUILD_P11 AND TEST_PKCS11_MODULE_PATH)
        add_test(NAME device_cred_prov_hsm_test COMMAND ${PROJECT_SOURCE_DIR}/tests/device_cred_prov_hsm_test.py
            --build-dir ${PROJECT_BINARY_DIR}
            --src-dir ${PROJECT_SOURCE_DIR}
            --credentials ${SOTA_PACKED_CREDENTIALS}
            --pkcs11-module ${TEST_PKCS11_MODULE_PATH}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests
            )
        set_tests_properties(device_cred_prov_hsm_test PROPERTIES LABELS "credentials")
    endif(BUILD_P11 AND TEST_PKCS11_MODULE_PATH)
endif(SOTA_PACKED_CREDENTIALS)

if(BUILD_DEB)
    add_test(NAME memory_test COMMAND ${PROJECT_SOURCE_DIR}/tests/memory_usage_test.sh
             ${PROJECT_BINARY_DIR} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif(BUILD_DEB)

###############################################################################
# The test feature of cmake checks the return value when the program
# exits. If the return value is zero, the testcase passes.

# test running the executable with command line option --help
add_test(NAME test_cmdline--help COMMAND aktualizr --help)
# test running the executable with command line option -h
add_test(NAME test_cmdline-h COMMAND aktualizr -h)
# test running the executable with command line option --something
add_test(NAME test_cmdline--something
         COMMAND aktualizr --something -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml)
# test running the executable with command line option -sth
add_test(NAME test_cmdline-sth
         COMMAND aktualizr -sth -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml)
# test the return code when running the executable with non-existent configuration file
add_test(NAME test_no_config_check_code
         COMMAND aktualizr -c non-existent-config.toml)

# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass
set_tests_properties(test_cmdline--something
                     test_cmdline-sth
                     test_no_config_check_code
                     PROPERTIES WILL_FAIL TRUE)

# Check for expected output messages given specific misconfigurations
# test the error message when running the executable with non-existent configuration file
add_test(NAME test-no-config-check-message
         COMMAND aktualizr -c non-existent-config.toml)
set_tests_properties(test-no-config-check-message
                     PROPERTIES PASS_REGULAR_EXPRESSION "Config directory non-existent-config.toml does not exist.")

add_test(NAME test-help-with-other-options
         COMMAND aktualizr --help -c someconfig.toml)
set_tests_properties(test-help-with-other-options
                     PROPERTIES PASS_REGULAR_EXPRESSION "aktualizr command line options")

add_test(NAME test-help-with-nonexistent-options
         COMMAND aktualizr --help -somebadoption)
set_tests_properties(test-help-with-nonexistent-options
                     PROPERTIES PASS_REGULAR_EXPRESSION "aktualizr command line options")

# run `aktualizr check` for these tests, as they can run forever if a global
# configuration is present

# Check verbose config parsing output with debug loglevel.
add_test(NAME test_log_debug
         COMMAND aktualizr -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml --loglevel=0 check)
set_tests_properties(test_log_debug
                     PROPERTIES PASS_REGULAR_EXPRESSION "Final configuration that will be used")

# Check silent config parsing output with default loglevel. Note that the extra
# PASS is necessary to ignore the return code.
add_test(NAME test_log_default
         COMMAND aktualizr -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml check)
set_tests_properties(test_log_default
                     PROPERTIES FAIL_REGULAR_EXPRESSION "Final configuration that will be used"
                                PASS_REGULAR_EXPRESSION "Aktualizr version")

# Check invalid logging levels.
add_test(NAME test_log_invalid
         COMMAND aktualizr -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml --loglevel=6 check)
set_tests_properties(test_log_invalid
                     PROPERTIES PASS_REGULAR_EXPRESSION "Invalid log level")
add_test(NAME test_log_negative
         COMMAND aktualizr -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml --loglevel=-1 check)
set_tests_properties(test_log_negative
                     PROPERTIES PASS_REGULAR_EXPRESSION "Invalid log level")

if (BUILD_OSTREE)
    set(TEST_IPSEC_ARGS "--ostree")
endif ()
add_test(NAME test_ip_secondary
         COMMAND ${PROJECT_SOURCE_DIR}/tests/ipsecondary_test.py
         --build-dir ${PROJECT_BINARY_DIR} --src-dir ${PROJECT_SOURCE_DIR} ${TEST_IPSEC_ARGS})

add_test(NAME test_director_failure
        COMMAND ${PROJECT_SOURCE_DIR}/tests/test_director_failure.py
        --build-dir ${PROJECT_BINARY_DIR} --src-dir ${PROJECT_SOURCE_DIR})
set_tests_properties(test_director_failure PROPERTIES LABELS "noptest")

add_test(NAME test_imagerepo_failure
        COMMAND ${PROJECT_SOURCE_DIR}/tests/test_imagerepo_failure.py
        --build-dir ${PROJECT_BINARY_DIR} --src-dir ${PROJECT_SOURCE_DIR})
set_tests_properties(test_imagerepo_failure PROPERTIES LABELS "noptest")

add_test(NAME test_customrepo_failure
         COMMAND ${PROJECT_SOURCE_DIR}/tests/test_customrepo_failure.py
         --build-dir ${PROJECT_BINARY_DIR} --src-dir ${PROJECT_SOURCE_DIR})
set_tests_properties(test_customrepo_failure PROPERTIES LABELS "noptest")

if(BUILD_OSTREE)
    add_test(NAME test_treehub_failure
             COMMAND ${PROJECT_SOURCE_DIR}/tests/test_treehub_failure.py
             --build-dir ${PROJECT_BINARY_DIR} --src-dir ${PROJECT_SOURCE_DIR})
    set_tests_properties(test_treehub_failure PROPERTIES LABELS "noptest")

    add_test(NAME test_update
             COMMAND ${PROJECT_SOURCE_DIR}/tests/test_update.py
             --build-dir ${PROJECT_BINARY_DIR} --src-dir ${PROJECT_SOURCE_DIR})
    set_tests_properties(test_update PROPERTIES LABELS "noptest")
endif(BUILD_OSTREE)

add_test(NAME test_aktualizr_kill
         COMMAND ${PROJECT_SOURCE_DIR}/tests/test_aktualizr_kill.py
         --build-dir ${PROJECT_BINARY_DIR} --src-dir ${PROJECT_SOURCE_DIR})
set_tests_properties(test_aktualizr_kill PROPERTIES LABELS "noptest")

add_test(NAME test_install_aktualizr_and_update COMMAND ${PROJECT_SOURCE_DIR}/tests/test_install_aktualizr_and_update.sh ${PROJECT_BINARY_DIR} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(test_install_aktualizr_and_update PROPERTIES LABELS "noptest")

add_executable(aktualizr-cycle-simple aktualizr_cycle_simple.cc)
target_link_libraries(aktualizr-cycle-simple aktualizr_lib)
aktualizr_source_file_checks(aktualizr_cycle_simple.cc)
add_dependencies(build_tests aktualizr-cycle-simple)

if(FAULT_INJECTION)
    # run with a very small amount of tests on CI, should be more useful when
    # run for several hours
    add_test(NAME test_io_failure COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_io_failure.py
        --uptane-gen $<TARGET_FILE:uptane-generator> --akt-test $<TARGET_FILE:aktualizr-cycle-simple> -n 50 -j 10
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif(FAULT_INJECTION)

add_dependencies(qa check)

aktualizr_source_file_checks(${TEST_SOURCES})

# vim: set tabstop=4 shiftwidth=4 expandtab:
