if(NOT GTEST_ROOT )
    set(GTEST_ROOT ${PROJECT_SOURCE_DIR}/third_party/googletest/googletest)
endif()
if(NOT GMOCK_ROOT )
    set(GMOCK_ROOT ${PROJECT_SOURCE_DIR}/third_party/googletest/googlemock)
endif()

# Temporarily remove flags not supported by gtest.
remove_definitions(-Wswitch-default -Wconversion)
add_subdirectory(${GMOCK_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gmock EXCLUDE_FROM_ALL)
add_definitions(-Wswitch-default -Wconversion)
add_subdirectory(uptane_repo_generation)

include_directories("${PROJECT_SOURCE_DIR}/src/libaktualizr/third_party/jsoncpp")

# use the same libraries as the normal target but add our test utility library, gtest, gmock and gcov
set(TEST_LIBS
    ${AKTUALIZR_EXTERNAL_LIBS}
    testutilities
    gtest gmock)

add_dependencies(build_tests aktualizr)
if(BUILD_SOTA_TOOLS)
    add_dependencies(build_tests garage-push)
    add_dependencies(build_tests garage-check)
endif(BUILD_SOTA_TOOLS)
add_dependencies(build_tests aktualizr-secondary)
add_dependencies(build_tests aktualizr-info)
add_dependencies(build_tests aktualizr-repo)

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
                  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
                  )


# List of source files to run static analysis on. Automatically appended to by
# add_aktualizr_test, but anything that doesn't use that must be manually added
# to this list.
set(TEST_SOURCES httpfake.h test_utils.cc test_utils.h uptane_vector_tests.cc)


include(CMakeParseArguments)

add_library(testutilities test_utils.cc)

# Setup coverage
if(BUILD_WITH_CODE_COVERAGE)
    add_definitions(${COVERAGE_COMPILER_FLAGS})
    setup_target_for_coverage_lcov(NAME coverage EXECUTABLE ${CMAKE_CTEST_COMMAND} ${CTEST_EXTRA_ARGS})
    add_dependencies(coverage build_tests)
endif(BUILD_WITH_CODE_COVERAGE)

if(CMAKE_BUILD_TYPE MATCHES "Valgrind")
    # This test is only checking that valgrind finds memory leaks
    add_aktualizr_test(NAME leak SOURCES leak_test.cc)
    set_tests_properties(test_leak PROPERTIES WILL_FAIL TRUE)
endif()


add_executable(aktualizr_uptane_vector_tests uptane_vector_tests.cc)
target_link_libraries(aktualizr_uptane_vector_tests aktualizr_static_lib ${TEST_LIBS})

if(CMAKE_BUILD_TYPE MATCHES "Valgrind")
    set(VECTOR_TESTS_ARGS "valgrind")
endif(CMAKE_BUILD_TYPE MATCHES "Valgrind")
add_test(NAME test_uptane_vectors COMMAND ${PROJECT_SOURCE_DIR}/tests/run_vector_tests.sh
    ${PROJECT_SOURCE_DIR}/tests ${VECTOR_TESTS_ARGS} ${GOOGLE_TEST_OUTPUT})
add_dependencies(build_tests aktualizr_uptane_vector_tests)
set_tests_properties(test_uptane_vectors PROPERTIES LABELS "uptane_vectors")

if(SOTA_PACKED_CREDENTIALS)
    add_test(NAME auto_prov_test COMMAND ${PROJECT_SOURCE_DIR}/tests/auto_prov_test.py
             --build-dir ${PROJECT_BINARY_DIR}
             --credentials ${SOTA_PACKED_CREDENTIALS})
    set_tests_properties(auto_prov_test PROPERTIES LABELS "credentials")

    add_test(NAME implicit_prov_test COMMAND ${PROJECT_SOURCE_DIR}/tests/implicit_prov_test.py
             --build-dir ${PROJECT_BINARY_DIR}
             --credentials ${SOTA_PACKED_CREDENTIALS})
    set_tests_properties(implicit_prov_test PROPERTIES LABELS "credentials")

    if(BUILD_P11 AND TEST_PKCS11_MODULE_PATH)
        add_test(NAME hsm_prov_test COMMAND ${PROJECT_SOURCE_DIR}/tests/hsm_prov_test.py
            --build-dir ${PROJECT_BINARY_DIR}
            --src-dir ${PROJECT_SOURCE_DIR}
            --credentials ${SOTA_PACKED_CREDENTIALS}
            --pkcs11-module ${TEST_PKCS11_MODULE_PATH}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests
            )
        set_tests_properties(hsm_prov_test PROPERTIES LABELS "credentials")
    endif(BUILD_P11 AND TEST_PKCS11_MODULE_PATH)
endif(SOTA_PACKED_CREDENTIALS)


add_test(NAME memory_test COMMAND ${PROJECT_SOURCE_DIR}/tests/memory_usage_test.sh ${PROJECT_BINARY_DIR}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
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
# call the executable without any options
add_test(NAME test_cmdline_empty COMMAND aktualizr)
# test the return code when running the executable with non-existent configuration file
add_test(NAME test_no_config_check_code
         COMMAND aktualizr -c non-existent-config.toml)

# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass
set_tests_properties(test_cmdline--something
                     test_cmdline-sth
                     test_cmdline_empty
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

add_test(NAME test-secondary-config-with-nonexisting-dir
         COMMAND aktualizr -c ${PROJECT_SOURCE_DIR}/config/sota_autoprov.toml
         --tls-server fake --secondary-configs-dir nonexistingdir)
set_tests_properties(test-secondary-config-with-nonexisting-dir
                     PROPERTIES PASS_REGULAR_EXPRESSION "\"nonexistingdir\": not a directory")

# Check verbose config parsing output with debug loglevel.
add_test(NAME test_log_debug
         COMMAND aktualizr -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml --loglevel=0)
set_tests_properties(test_log_debug
                     PROPERTIES PASS_REGULAR_EXPRESSION "Final configuration that will be used")
# Check silent config parsing output with default loglevel. Note that the extra
# PASS is necessary to ignore the return code.
add_test(NAME test_log_default
         COMMAND aktualizr -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml)
set_tests_properties(test_log_default
                     PROPERTIES FAIL_REGULAR_EXPRESSION "Final configuration that will be used"
                                PASS_REGULAR_EXPRESSION "Aktualizr version")

# Check invalid logging levels.
add_test(NAME test_log_invalid
         COMMAND aktualizr -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml --loglevel=6)
set_tests_properties(test_log_invalid
                     PROPERTIES PASS_REGULAR_EXPRESSION "Invalid log level")
add_test(NAME test_log_negative
         COMMAND aktualizr -c ${PROJECT_SOURCE_DIR}/tests/config/minimal.toml --loglevel=-1)
set_tests_properties(test_log_negative
                     PROPERTIES PASS_REGULAR_EXPRESSION "Invalid log level")

add_dependencies(qa check)

aktualizr_source_file_checks(${TEST_SOURCES})

# vim: set tabstop=4 shiftwidth=4 expandtab:
