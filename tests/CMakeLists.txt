find_program(VALGRIND NAMES valgrind)

if(NOT GTEST_ROOT )
    set(GTEST_ROOT ${PROJECT_SOURCE_DIR}/third_party/googletest/googletest)
endif()

if(NOT GMOCK_ROOT )
    set(GMOCK_ROOT ${PROJECT_SOURCE_DIR}/third_party/googletest/googlemock)
endif()
add_subdirectory(${GMOCK_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gmock EXCLUDE_FROM_ALL)

# use the same libraries as the normal target but add our test utility library, gtest, gmock and gcov
set (TEST_LIBS
    ${Boost_SYSTEM_LIBRARIES}
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${sodium_LIBRARY_RELEASE}
    ${LIBOSTREE_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${LIBDBUS_LIBRARIES}
    ${LibArchive_LIBRARIES}
    ${LIBP11_LIBRARIES}
    ${LIBDPKG_LIBRARIES}
    testutilities
    gtest gmock)

if(BUILD_WITH_CODE_COVERAGE)
    list(APPEND TEST_LIBS gcov)
endif(BUILD_WITH_CODE_COVERAGE)

add_dependencies(build_tests aktualizr)


if(BUILD_SOTA_TOOLS)
    add_dependencies(build_tests garage-push)
endif(BUILD_SOTA_TOOLS)

add_dependencies(build_tests aktualizr-info)

add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=1 ${CMAKE_CTEST_COMMAND}
                  -E test_valgrind_uptane_vectors\\|test_build DEPENDS build_tests)
add_custom_target(check-full COMMAND CTEST_OUTPUT_ON_FAILURE=1 ${CMAKE_CTEST_COMMAND}
                  -E test_uptane_vectors DEPENDS build_tests)

# Reference asn1c libraries
add_asn1c_lib(config asn1-reference/common asn1-reference/config/cryptosource asn1-reference/config/tlsconfig)
list(APPEND TEST_LIBS config_asn1)

# List of source files to run clang-format and clang-check on. Automatically
# appended to by add_aktualizr_test, but anything that doesn't use that must be
# manually added to this list.
set(TEST_SOURCES httpfake.h test_utils.cc test_utils.h)


include(CMakeParseArguments)

function(add_aktualizr_test)
    set(options PROJECT_WORKING_DIRECTORY NO_VALGRIND)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES ARGS)
    cmake_parse_arguments(AKTUALIZR_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    add_executable(t_${AKTUALIZR_TEST_NAME} ${AKTUALIZR_TEST_SOURCES})
    target_link_libraries(t_${AKTUALIZR_TEST_NAME} aktualizr_static_lib ${TEST_LIBS})
    if(AKTUALIZR_TEST_PROJECT_WORKING_DIRECTORY)
        set(WD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    else()
        set(WD )
    endif()
        # Running tests that are both instrumented by gcov and valgrind at
        # the same time is very expensive.  Disable valgrind when running
        # under gcov, or when the test is explicitly flagged that it fails
        # under valgrind (these should be fixed)
        if(AKTUALIZR_TEST_NO_VALGRIND OR BUILD_WITH_CODE_COVERAGE)
            add_test(NAME test_${AKTUALIZR_TEST_NAME}
                     COMMAND t_${AKTUALIZR_TEST_NAME} ${AKTUALIZR_TEST_ARGS}
                     ${WD})
        else()
            add_test(NAME test_${AKTUALIZR_TEST_NAME}
                     COMMAND ${RUN_VALGRIND} ${CMAKE_CURRENT_BINARY_DIR}/t_${AKTUALIZR_TEST_NAME} ${AKTUALIZR_TEST_ARGS}
                     ${WD})
        endif()
    add_dependencies(build_tests t_${AKTUALIZR_TEST_NAME})
    set(TEST_SOURCES ${TEST_SOURCES} ${AKTUALIZR_TEST_SOURCES} PARENT_SCOPE)
endfunction(add_aktualizr_test)


add_library(testutilities test_utils.cc)

# Setup coverage
if(BUILD_WITH_CODE_COVERAGE)
    include(CodeCoverage)
    setup_target_for_coverage(coverage ${CMAKE_CTEST_COMMAND} coverage)
    add_definitions(-g -O0 -fprofile-arcs -ftest-coverage)
    target_compile_options(aktualizr_static_lib PUBLIC -fprofile-arcs -ftest-coverage)
    add_dependencies(coverage build_tests)
endif(BUILD_WITH_CODE_COVERAGE)

# Setup PKCS11
if(TEST_PKCS11_MODULE_PATH)
    add_definitions(-DTEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}"
                    -DTEST_PKCS11_ENGINE_PATH="${TEST_PKCS11_ENGINE_PATH}")
endif(TEST_PKCS11_MODULE_PATH)

configure_file(run-valgrind.in run-valgrind @ONLY)
SET(RUN_VALGRIND ${CMAKE_CURRENT_BINARY_DIR}/run-valgrind)

if(NOT BUILD_WITH_CODE_COVERAGE)
    # Code coverage disables valgrind, and this test is only checking that
    # valgrind finds memory leaks
    add_aktualizr_test(NAME leak SOURCES leak_test.cc)
    set_tests_properties(test_leak PROPERTIES WILL_FAIL TRUE)
endif()

add_aktualizr_test(NAME commands SOURCES commands_test.cc)

add_aktualizr_test(NAME config SOURCES config_test.cc ARGS ${PROJECT_BINARY_DIR} PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME crypto SOURCES crypto_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME events SOURCES events_test.cc)

add_aktualizr_test(NAME http_client SOURCES httpclient_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME keymanager SOURCES keymanager_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME packagemanager SOURCES packagemanager_test.cc NO_VALGRIND
                   ARGS ${PROJECT_BINARY_DIR}/ostree_repo)

add_aktualizr_test(NAME socket_gateway SOURCES socketgateway_test.cc
                   ARGS ${PROJECT_SOURCE_DIR}/tests/fake_unix_socket/)

add_aktualizr_test(NAME sqlstorage SOURCES sqlstorage_test.cc PROJECT_WORKING_DIRECTORY
                   ARGS ${PROJECT_SOURCE_DIR}/config/schemas)

add_aktualizr_test(NAME storage SOURCES storage_common_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME timer SOURCES timer_test.cc)

add_aktualizr_test(NAME tuf SOURCES tuf_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME utils SOURCES utils_test.cc)

add_aktualizr_test(NAME uptane SOURCES uptane_test.cc PROJECT_WORKING_DIRECTORY
                       ARGS ${PROJECT_BINARY_DIR}/ostree_repo)

add_aktualizr_test(NAME uptane_implicit SOURCES uptane_implicit_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME uptane_key SOURCES uptane_key_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME uptane_network SOURCES uptane_network_test.cc PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME uptane_serial SOURCES uptane_serial_test.cc ARGS ${PROJECT_BINARY_DIR}
                   PROJECT_WORKING_DIRECTORY)

add_aktualizr_test(NAME uptane_secondary SOURCES uptane_secondary_test.cc)

add_aktualizr_test(NAME asn1 SOURCES asn1_test.cc ARGS ${PROJECT_BINARY_DIR} PROJECT_WORKING_DIRECTORY)

add_executable(aktualizr_uptane_vector_tests uptane_vector_tests.cc)
target_link_libraries(aktualizr_uptane_vector_tests aktualizr_static_lib ${TEST_LIBS} crypto)
set(TEST_SOURCES ${TEST_SOURCES} uptane_vector_tests.cc)

add_test(NAME test_uptane_vectors COMMAND ${PROJECT_SOURCE_DIR}/tests/run_vector_tests.sh
         ${PROJECT_SOURCE_DIR}/tests/tuf-test-vectors)
add_test(NAME test_valgrind_uptane_vectors COMMAND ${PROJECT_SOURCE_DIR}/tests/run_vector_tests.sh
         ${PROJECT_SOURCE_DIR}/tests/tuf-test-vectors valgrind)
add_dependencies(build_tests aktualizr_uptane_vector_tests)

if(BUILD_OSTREE)
    add_custom_target(make_ostree_sysroot
                      COMMAND ${PROJECT_SOURCE_DIR}/scripts/make_ostree_sysroot.sh
                      ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
    add_dependencies(build_tests make_ostree_sysroot)

    add_aktualizr_test(NAME ostree SOURCES ostree_test.cc PROJECT_WORKING_DIRECTORY NO_VALGRIND
                       ARGS ${PROJECT_BINARY_DIR}/ostree_repo)

    if(SOTA_PACKED_CREDENTIALS)
        add_aktualizr_test(NAME uptane_ci SOURCES uptane_ci_test.cc PROJECT_WORKING_DIRECTORY NO_VALGRIND
                           ARGS ${SOTA_PACKED_CREDENTIALS} ${PROJECT_BINARY_DIR}/ostree_repo)
    endif(SOTA_PACKED_CREDENTIALS)
endif(BUILD_OSTREE)

if(BUILD_OPCUA)
    set_source_files_properties(opcuabridge_test_utils.cc opcuabridge_messaging_test.cc opcuabridge_server.cc
                                opcuabridge_secondary.cc opcuabridge_secondary_update_test.cc
                                PROPERTIES COMPILE_FLAGS ${OPEN62541_IGNORED_WARNINGS})
    add_executable(opcuabridge-server opcuabridge_server.cc opcuabridge_test_utils.cc)
    target_link_libraries(opcuabridge-server aktualizr_static_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
    add_dependencies(build_tests opcuabridge-server)

    add_aktualizr_test(NAME opcuabridge_messaging
                       SOURCES opcuabridge_messaging_test.cc opcuabridge_test_utils.cc)

    add_executable(opcuabridge-secondary opcuabridge_secondary.cc opcuabridge_test_utils.cc)
    target_link_libraries(opcuabridge-secondary aktualizr_static_lib
                          ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${sodium_LIBRARY_RELEASE})
    add_dependencies(build_tests opcuabridge-secondary)

    add_aktualizr_test(NAME opcuabridge_secondary_update
                       SOURCES opcuabridge_secondary_update_test.cc opcuabridge_test_utils.cc)
    set(TEST_SOURCES ${TEST_SOURCES} opcuabridge_messaging_test.cc opcuabridge_server.cc
                     opcuabridge_secondary.cc opcuabridge_secondary_update_test.cc
                     opcuabridge_test_utils.cc)
endif(BUILD_OPCUA)

if(BUILD_DEB)
    add_executable(t_packagemanager_deb packagemanager_deb_tests.cc)
    add_dependencies(build_tests t_packagemanager_deb)
    target_link_libraries(t_packagemanager_deb aktualizr_static_lib ${TEST_LIBS})

    add_test(NAME test_packagemanager_deb COMMAND ${PROJECT_SOURCE_DIR}/tests/run_debian_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/t_packagemanager_deb
    ${PROJECT_SOURCE_DIR}/tests/test_data/fake_dpkg)
    list(APPEND TEST_SOURCES packagemanager_deb_tests.cc)
endif(BUILD_DEB)

###############################################################################
# The test feature of cmake checks the return value when the program
# exits. If the return value is zero, the testcase passes.

# test running the executable with command line option --help
add_test(NAME feat1_test--help COMMAND aktualizr --help)
# test running the executable with command line option -h
add_test(NAME feat1_test-h COMMAND aktualizr -h)
# test running the executable with command line option --something
add_test(NAME feat1_test--something
         COMMAND aktualizr --something -c ${PROJECT_SOURCE_DIR}/config/config.toml.example)
# test running the executable with command line option -sth
add_test(NAME feat1_test-sth
         COMMAND aktualizr -sth -c ${PROJECT_SOURCE_DIR}/config/config.toml.example)
# call the executable without any options
add_test(NAME test_plain COMMAND aktualizr)
# check invalid logging level
add_test(NAME test_log_invalid COMMAND aktualizr --loglevel=5)
# test the return code when running the executable with non-existent configuration file
add_test(NAME test-no-config-check-code
         COMMAND aktualizr -c non-existent-config.toml)

# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass
set_tests_properties(feat1_test--something
                     feat1_test-sth
                     test_plain
                     test_log_invalid
                     test-no-config-check-code
                     PROPERTIES WILL_FAIL TRUE)

# test the error message when running the executable with non-existent configuration file
add_test(NAME test-no-config-check-message
         COMMAND aktualizr -c non-existent-config.toml)

add_test(NAME test-help-with-other-options
         COMMAND aktualizr --help -c someconfig.toml)

add_test(NAME test-help-with-nonexistent-options
         COMMAND aktualizr --help -somebadoption)

set_tests_properties(test-no-config-check-message
                     PROPERTIES PASS_REGULAR_EXPRESSION  "aktualizr: configuration file .* not found. Exiting.")

set_tests_properties(test-help-with-other-options
                     PROPERTIES PASS_REGULAR_EXPRESSION  "aktualizr command line options")

set_tests_properties(test-help-with-nonexistent-options
                     PROPERTIES PASS_REGULAR_EXPRESSION  "aktualizr command line options")

add_test(NAME test-legacy-interface-with-nonexistent-file
         COMMAND aktualizr -c  ${PROJECT_SOURCE_DIR}/config/config.toml.example --legacy-interface nonexistentfile)

set_tests_properties(test-legacy-interface-with-nonexistent-file
                     PROPERTIES PASS_REGULAR_EXPRESSION  "Legacy external flasher not found: nonexistentfile")

add_test(NAME test-secondary-config-with-nonexistent-file
         COMMAND aktualizr -c  ${PROJECT_SOURCE_DIR}/config/config.toml.example --secondary-config nonexistentfile)

set_tests_properties(test-secondary-config-with-nonexistent-file
                     PROPERTIES PASS_REGULAR_EXPRESSION  "nonexistentfile does not exist!")

# Try building with various cmake options
add_test(NAME test_build_all_off
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options.sh
        ${PROJECT_SOURCE_DIR} test_build_all_off "-DBUILD_OSTREE=OFF -DBUILD_DEB=OFF")

add_test(NAME test_build_all_on
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options.sh
        ${PROJECT_SOURCE_DIR} test_build_all_on "-DBUILD_WITH_CODE_COVERAGE=ON -DBUILD_OSTREE=ON -DBUILD_DEB=ON")

add_test(NAME test_build_debug
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options.sh
        ${PROJECT_SOURCE_DIR} test_build_debug "-DCMAKE_BUILD_TYPE=Debug")

add_test(NAME test_build_release_no_ostree
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options.sh
        ${PROJECT_SOURCE_DIR} test_build_release_no_ostree "-DCMAKE_BUILD_TYPE=Release -DBUILD_OSTREE=off")

add_test(NAME test_build_release
        COMMAND ${PROJECT_SOURCE_DIR}/tests/build_with_options.sh
        ${PROJECT_SOURCE_DIR} test_build_release "-DCMAKE_BUILD_TYPE=Release -DBUILD_OSTREE=on")

add_dependencies(qa check-full)


# implicit_writer tests
add_test(NAME implicit-writer-help COMMAND aktualizr_implicit_writer --help)

if(SOTA_PACKED_CREDENTIALS)
    set(IMPLICIT_CREDENTIALS ${SOTA_PACKED_CREDENTIALS})
else()
    set(IMPLICIT_CREDENTIALS ${PROJECT_SOURCE_DIR}/tests/test_data/credentials.zip)
endif(SOTA_PACKED_CREDENTIALS)
add_test(NAME implicit-writer-test
    COMMAND ${PROJECT_SOURCE_DIR}/tests/implicit_writer_test.py
    -c ${IMPLICIT_CREDENTIALS} -i ${PROJECT_SOURCE_DIR}/config/sota_implicit_prov.toml
    -o ${PROJECT_BINARY_DIR}/implicit -w ${PROJECT_BINARY_DIR}/src/implicit_writer/)


# external_secondaries tests
add_dependencies(build_tests aktualizr-validate-secondary-interface example-interface)
add_test(NAME validate-secondary-interface
    COMMAND aktualizr-validate-secondary-interface
    --target ${PROJECT_BINARY_DIR}/src/external_secondaries/example-interface
    --firmware ${PROJECT_SOURCE_DIR}/tests/test_data/firmware.txt)


if((BUILD_SOTA_TOOLS))
    # sota-tools tests
    add_aktualizr_test(NAME sota_tools_auth_test SOURCES sota_tools/auth_test.cc
                       PROJECT_WORKING_DIRECTORY NO_VALGRIND)
    target_link_libraries(t_sota_tools_auth_test sota_tools_static_lib aktualizr_static_lib ${TEST_LIBS})
    target_include_directories(t_sota_tools_auth_test
                               PUBLIC ${PROJECT_SOURCE_DIR}/src/sota_tools
                               ${PROJECT_SOURCE_DIR}/tests ${GLIB2_INCLUDE_DIRS})

    add_aktualizr_test(NAME ostree_hash SOURCES sota_tools/ostree_hash_test.cc)
    target_link_libraries(t_ostree_hash sota_tools_static_lib)

    # Setup coverage for sota_tools_static_lib
    if(BUILD_WITH_CODE_COVERAGE)
        target_compile_options(sota_tools_static_lib PUBLIC -fprofile-arcs -ftest-coverage)
    endif(BUILD_WITH_CODE_COVERAGE)

    # Check the --help option works
    add_test(NAME option-help
        COMMAND garage-push --help)

    # Invalid arguments result in a failure
    add_test(NAME bad-option
        COMMAND sota_tools/test-bad-option $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Detect that the thing we passed in to --repo is not a repository
    add_test(NAME invalid-repo
        COMMAND sota_tools/test-invalid-repo $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Detect a --repo that isn't in 'archive-z2' mode
    add_test(NAME bare-mode-repo
        COMMAND sota_tools/test-bare-mode-repo $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Detect that the ref we passed in to --ref is not present
    add_test(NAME missing-ref
        COMMAND sota_tools/test-missing-ref $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Handle a missing credentials.zip
    add_test(NAME missing-credentials
        COMMAND sota_tools/test-missing-credentials $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Handle passing a random file as credentials.zip
    add_test(NAME invalid-credentials
        COMMAND sota_tools/test-invalid-credentials $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_test(NAME cacert-not-found
        COMMAND sota_tools/test-cacert-not-found $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_test(NAME server-500
        COMMAND ./test-server-500 $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sota_tools)

    add_test(NAME auth-plus-failure
        COMMAND sota_tools/test-auth-plus-failure $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    if(SOTA_PACKED_CREDENTIALS)
        add_test(NAME dry-run
            COMMAND sota_tools/test-dry-run $<TARGET_FILE:garage-push>
            ${SOTA_PACKED_CREDENTIALS} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

        add_test(NAME auth-plus-happy-case
            COMMAND sota_tools/test-auth-plus-happy $<TARGET_FILE:garage-push>
            ${SOTA_PACKED_CREDENTIALS} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

        add_test(NAME verbose-logging
            COMMAND sota_tools/test-verbose-logging $<TARGET_FILE:garage-push>
            ${SOTA_PACKED_CREDENTIALS} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

        if(STRACE)
            add_test(NAME cacert-used
                COMMAND sota_tools/test-cacert-used $<TARGET_FILE:garage-push>
                ${SOTA_PACKED_CREDENTIALS} ${STRACE} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif(STRACE)
    endif(SOTA_PACKED_CREDENTIALS)

    # garage-deploy tests
    add_test(NAME garage-deploy-option-help
        COMMAND garage-deploy --help)

    add_test(NAME garage-deploy-nonexistent-option
        COMMAND garage-deploy --test )
    set_tests_properties(garage-deploy-nonexistent-option PROPERTIES WILL_FAIL TRUE)
endif((BUILD_SOTA_TOOLS))

# aktualizr-info tests
add_test(NAME aktualizr-info-noarguments
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${PROJECT_BINARY_DIR}/src/aktualizr_info/aktualizr-info WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(aktualizr-info-noarguments PROPERTIES
    PASS_REGULAR_EXPRESSION "Device ID: new-kartoffelpuffer-706\nPrimary ecu serial ID: cd51a49a-25d9-438e-93fa-c0dde6bfcec9")

add_test(NAME aktualizr-info-images-root
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${PROJECT_BINARY_DIR}/src/aktualizr_info/aktualizr-info --images-root WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
set_tests_properties(aktualizr-info-images-root PROPERTIES
    PASS_REGULAR_EXPRESSION "image root.json content:\n\n\\{.*\\}")

add_test(NAME aktualizr-info-director-root
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${PROJECT_BINARY_DIR}/src/aktualizr_info/aktualizr-info --director-root WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
set_tests_properties(aktualizr-info-director-root PROPERTIES
    PASS_REGULAR_EXPRESSION "director root.json content:\n\n\\{.*\\}")

add_test(NAME aktualizr-info-director-target
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${PROJECT_BINARY_DIR}/src/aktualizr_info/aktualizr-info --director-target WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
set_tests_properties(aktualizr-info-director-target PROPERTIES
    PASS_REGULAR_EXPRESSION "director targets.json content:\n\n\\{.*\\}")

add_test(NAME aktualizr-info-images-target
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${PROJECT_BINARY_DIR}/src/aktualizr_info/aktualizr-info --images-target WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
set_tests_properties(aktualizr-info-images-target PROPERTIES
    PASS_REGULAR_EXPRESSION "image targets.json content:\n\n\\{.*\\}")

# cert_provider test
add_test(NAME certprovider-ca COMMAND ${PROJECT_SOURCE_DIR}/tests/run_certprovider_test.sh ${PROJECT_BINARY_DIR}/src/cert_provider/aktualizr_cert_provider ${PROJECT_SOURCE_DIR}/tests/test_data/credentials.zip ${PROJECT_SOURCE_DIR}/tests/test_data/CAcert.pem ${PROJECT_SOURCE_DIR}/tests/test_data/CApkey.pem)

# config file test for collisions between import and FS->SQL migration paths
add_test(NAME config-import COMMAND ${PROJECT_SOURCE_DIR}/tests/run_import_clash_test.sh ${PROJECT_SOURCE_DIR}/config)


list(REMOVE_DUPLICATES TEST_SOURCES)
# message("${TEST_SOURCES}")
aktualizr_source_file_checks(${TEST_SOURCES})

# vim: set tabstop=4 shiftwidth=4 expandtab:
