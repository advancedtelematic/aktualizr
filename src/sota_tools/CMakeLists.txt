set(SOTA_TOOLS_LIB_SRC
    authenticate.cc
    check.cc
    deploy.cc
    garage_tools_version.cc
    oauth2.cc
    ostree_dir_repo.cc
    ostree_hash.cc
    ostree_http_repo.cc
    ostree_object.cc
    ostree_ref.cc
    ostree_repo.cc
    rate_controller.cc
    request_pool.cc
    server_credentials.cc
    treehub_server.cc)

##### garage-push targets
set(GARAGE_PUSH_SRCS
    garage_push.cc)

if (BUILD_SOTA_TOOLS)
    set(SOTA_TOOLS_EXTERNAL_LIBS
        ${Boost_SYSTEM_LIBRARIES}
        ${Boost_LIBRARIES}
        ${JSONCPP_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${LibArchive_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${sodium_LIBRARY_RELEASE}
        ${GLIB2_LIBRARIES})

    set(GARAGE_TOOLS_VERSION "${AKTUALIZR_VERSION}")
    set_property(SOURCE garage_tools_version.cc PROPERTY COMPILE_DEFINITIONS GARAGE_TOOLS_VERSION="${GARAGE_TOOLS_VERSION}")
    add_library(sota_tools_lib SHARED ${SOTA_TOOLS_LIB_SRC})
    set_target_properties(sota_tools_lib PROPERTIES LIBRARY_OUTPUT_NAME sota_tools)
    target_include_directories(sota_tools_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR} ${GLIB2_INCLUDE_DIRS})

    # we link with aktualizr static lib here, to bundle everything in sota_tools_lib
    target_link_libraries(sota_tools_lib aktualizr_static_lib ${SOTA_TOOLS_EXTERNAL_LIBS})
    install(TARGETS sota_tools_lib LIBRARY DESTINATION lib COMPONENT garage_deploy)

    add_executable(garage-push ${GARAGE_PUSH_SRCS})
    target_link_libraries(garage-push sota_tools_lib)

    install(TARGETS garage-push RUNTIME DESTINATION bin COMPONENT garage_deploy)
endif (BUILD_SOTA_TOOLS)


##### garage-check targets
set(GARAGE_CHECK_SRCS
    garage_check.cc)

if (BUILD_SOTA_TOOLS)
    add_executable(garage-check ${GARAGE_CHECK_SRCS})

    target_link_libraries(garage-check sota_tools_lib)

    install(TARGETS garage-check RUNTIME DESTINATION bin COMPONENT garage_deploy)
endif (BUILD_SOTA_TOOLS)


##### garage-deploy targets
set(GARAGE_DEPLOY_SRCS
    garage_deploy.cc)

if (BUILD_SOTA_TOOLS)
    add_executable(garage-deploy ${GARAGE_DEPLOY_SRCS})
    target_link_libraries(garage-deploy sota_tools_lib)

    add_dependencies(build_tests garage-deploy)

    install(TARGETS garage-deploy RUNTIME DESTINATION bin COMPONENT garage_deploy)


    ### garage-sign targets
    if(GARAGE_SIGN_ARCHIVE)
        get_filename_component(GARAGE_SIGN_ARCHIVE_ABS ${GARAGE_SIGN_ARCHIVE} ABSOLUTE BASE_DIR ${CMAKE_BINARY_DIR})
        set(GARAGE_SIGN_ARGS ${GARAGE_SIGN_ARGS} "--archive" ${GARAGE_SIGN_ARCHIVE_ABS})
    else()
        if(GARAGE_SIGN_VERSION)
            set(GARAGE_SIGN_ARGS ${GARAGE_SIGN_ARGS} "--name" ${GARAGE_SIGN_VERSION})
        endif(GARAGE_SIGN_VERSION)
        if(GARAGE_SIGN_SHA256)
            set(GARAGE_SIGN_ARGS ${GARAGE_SIGN_ARGS} "--sha256" ${GARAGE_SIGN_SHA256})
        endif(GARAGE_SIGN_SHA256)
    endif()

    add_custom_target(garage-sign
                    COMMAND ${PROJECT_SOURCE_DIR}/scripts/get-garage-sign.py
                    --output ${CMAKE_CURRENT_BINARY_DIR}
                    ${GARAGE_SIGN_ARGS})
    add_dependencies(garage-deploy garage-sign)

    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/garage-sign/bin/garage-sign DESTINATION bin COMPONENT garage_deploy)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/garage-sign/lib DESTINATION . COMPONENT garage_deploy)
endif (BUILD_SOTA_TOOLS)


##### For clang-format
set(ALL_SOTA_TOOLS_HEADERS
    authenticate.h
    check.h
    deploy.h
    garage_common.h
    garage_tools_version.h
    oauth2.h
    ostree_dir_repo.h
    ostree_hash.h
    ostree_http_repo.h
    ostree_object.h
    ostree_ref.h
    ostree_repo.h
    rate_controller.h
    request_pool.h
    server_credentials.h
    treehub_server.h)

if (NOT BUILD_SOTA_TOOLS)
    set(TEST_SOURCES
        authenticate_test.cc
        deploy_test.cc
        ostree_dir_repo_test.cc
        ostree_hash_test.cc
        ostree_http_repo_test.cc
        ostree_object_test.cc
        rate_controller_test.cc
        treehub_server_test.cc)
endif(NOT BUILD_SOTA_TOOLS)


##### tests
if (BUILD_SOTA_TOOLS)

    set(TEST_CERT_DIR ${PROJECT_BINARY_DIR}/sota_tools/certs)
    set(TEST_CERTS "${TEST_CERT_DIR}/server.crt" "${TEST_CERT_DIR}/ca.crt" "${TEST_CERT_DIR}/good.zip" "${TEST_CERT_DIR}/bad.zip")

    add_custom_command(OUTPUT ${TEST_CERTS}
      COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/authentication/generate-certs.sh
      ${TEST_CERT_DIR})

    add_custom_target(sota_tools_cert_generation
      DEPENDS ${TEST_CERTS})

    # do not link tests with libaktualizr, but sota_tools_lib
    list(REMOVE_ITEM TEST_LIBS aktualizr_lib)
    list(INSERT TEST_LIBS 0 sota_tools_lib)

    ### common tests
    add_aktualizr_test(NAME sota_tools_auth_test
                       SOURCES authenticate_test.cc
                       PROJECT_WORKING_DIRECTORY
                       ARGS ${PROJECT_BINARY_DIR}/sota_tools/certs)
    add_dependencies(t_sota_tools_auth_test sota_tools_cert_generation)

    add_aktualizr_test(NAME ostree_hash
                       SOURCES ostree_hash_test.cc)

    add_aktualizr_test(NAME rate_controller
                       SOURCES rate_controller_test.cc)

    add_aktualizr_test(NAME ostree_dir_repo
                       SOURCES ostree_dir_repo_test.cc
                       PROJECT_WORKING_DIRECTORY)

    add_aktualizr_test(NAME ostree_http_repo
                       SOURCES ostree_http_repo_test.cc
                       PROJECT_WORKING_DIRECTORY)

    add_aktualizr_test(NAME treehub_server
                       SOURCES treehub_server_test.cc
                       PROJECT_WORKING_DIRECTORY)

    add_aktualizr_test(NAME deploy
                       SOURCES deploy_test.cc
                       PROJECT_WORKING_DIRECTORY)

    add_aktualizr_test(NAME ostree_object
                       SOURCES ostree_object_test.cc
                       PROJECT_WORKING_DIRECTORY)

    ### garage-check tests
    # Check the --help option works.
    add_test(NAME garage-check-option-help
        COMMAND garage-check --help)

    # Report version.
    add_test(NAME garage-check-option-version
        COMMAND garage-check --version)
    set_tests_properties(garage-check-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current garage-check version is: ${AKTUALIZR_VERSION}")

    # Verify that a commit exists in a remote repo.
    # Get targets.json from Image repository.
    # Find specified OSTree ref in targets.json.
    add_test(NAME check_not_expired_targets
        COMMAND ${PROJECT_SOURCE_DIR}/tests/run_expired_test.sh 2031-11-17T23:58:40Z ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    # Verify that a commit exists in a remote repo.
    # Get targets.json from Image repository.
    # Abort if targets.json has expired.
    add_test(NAME check_expired_targets
        COMMAND ${PROJECT_SOURCE_DIR}/tests/run_expired_test.sh 2017-11-17T23:58:40Z ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(check_expired_targets  PROPERTIES WILL_FAIL TRUE)


    ### garage-push tests
    # Check the --help option works.
    add_test(NAME garage-push-option-help
        COMMAND garage-push --help)

    # Report version.
    add_test(NAME garage-push-option-version
        COMMAND garage-push --version)
    set_tests_properties(garage-push-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current garage-push version is: ${AKTUALIZR_VERSION}")

    # Abort when given bogus command line options.
    add_test(NAME garage-push-bad-option
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-bad-option $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Abort when given a bogus OSTree ref.
    add_test(NAME garage-push-missing-ref
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-missing-ref $<TARGET_FILE:garage-push>
        ${SOTA_PACKED_CREDENTIALS}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    # Abort when given nonexistent credentials.
    add_test(NAME garage-push-missing-credentials
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-missing-credentials $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    # Abort when given bogus credentials.
    add_test(NAME garage-push-invalid-credentials
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-invalid-credentials $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    # Abort when given a bogus CA certificate.
    add_test(NAME garage-push-cacert-not-found
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-cacert-not-found $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    # Recover from the server hanging on to connections.
    add_test(NAME garage-push-server-500
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-server-500 $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/sota_tools)

    # Recover from intermittent errors.
    add_test(NAME garage-push-server-500-every-10-request
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-server-error_every_10 $<TARGET_FILE:garage-push> 500
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/sota_tools)

    # Recover from intermittent errors.
    add_test(NAME garage-push-server-409-every-10-request
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-server-error_every_10 $<TARGET_FILE:garage-push> 409
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/sota_tools)

    # Abort when server becomes unresponsive.
    add_test(NAME garage-push-server-500_after_20
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-server-500_after_20 $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/sota_tools)

    # Abort if authorization fails.
    add_test(NAME garage-push-auth-plus-failure
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-auth-plus-failure $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    if(SOTA_PACKED_CREDENTIALS)
        # Support dry run with auth plus using a real server.
        add_test(NAME garage-push-dry-run
            COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-dry-run $<TARGET_FILE:garage-push>
            ${SOTA_PACKED_CREDENTIALS} master
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
        set_tests_properties(garage-push-dry-run PROPERTIES LABELS "credentials")

        # Parse OSTree ref or commit refhash.
        add_test(NAME garage-push-dry-run-commit
            COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-dry-run $<TARGET_FILE:garage-push>
            ${SOTA_PACKED_CREDENTIALS} 16ef2f2629dc9263fdf3c0f032563a2d757623bbc11cf99df25c3c3f258dccbe
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
        set_tests_properties(garage-push-dry-run-commit PROPERTIES LABELS "credentials")

        # Support trace logging.
        add_test(NAME garage-push-trace-logging
            COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-trace-logging $<TARGET_FILE:garage-push>
            ${SOTA_PACKED_CREDENTIALS} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
        set_tests_properties(garage-push-trace-logging PROPERTIES LABELS "credentials")

        if(STRACE)
            # Use a provided CA certificate.
            add_test(NAME garage-push-cacert-used
                COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-cacert-used $<TARGET_FILE:garage-push>
                ${SOTA_PACKED_CREDENTIALS} ${STRACE} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
            set_tests_properties(garage-push-cacert-used PROPERTIES LABELS "credentials")
        endif(STRACE)
    endif(SOTA_PACKED_CREDENTIALS)


    ### garage-deploy tests
    # Check the --help option works.
    add_test(NAME garage-deploy-option-help
        COMMAND garage-deploy --help)

    # Report version.
    add_test(NAME garage-deploy-option-version
        COMMAND garage-deploy --version)
    set_tests_properties(garage-deploy-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current garage-deploy version is: ${AKTUALIZR_VERSION}")

    # Abort when given bogus command line options.
    add_test(NAME garage-deploy-bad-option
    COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-bad-option $<TARGET_FILE:garage-deploy>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Support debug logging.
    add_test(NAME garage-deploy-debug-logging
        COMMAND garage-deploy -f ${SOTA_PACKED_CREDENTIALS} -p ${SOTA_PACKED_CREDENTIALS} --commit 123 -h 3 --name 123 -v
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(garage-deploy-debug-logging PROPERTIES PASS_REGULAR_EXPRESSION "Debug level debugging enabled"  LABELS "credentials")

    # Support trace logging.
    add_test(NAME garage-deploy-trace-logging
        COMMAND garage-deploy -f ${SOTA_PACKED_CREDENTIALS} -p ${SOTA_PACKED_CREDENTIALS} --commit 16ef2f2629dc9263fdf3c0f032563a2d757623bbc11cf99df25c3c3f258dccbe -h 3 --name 123 --loglevel 0
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(garage-deploy-trace-logging PROPERTIES PASS_REGULAR_EXPRESSION "Loglevel set to 0"  LABELS "credentials")

    # Support dry run with local repos.
    add_test(NAME garage-deploy-dry-run
    COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-garage-deploy-dry-run $<TARGET_FILE:garage-deploy>
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(garage-deploy-dry-run PROPERTIES PASS_REGULAR_EXPRESSION "Dry run. No objects uploaded.")

    # Abort if credentials do not support offline signing.
    add_test(NAME garage-deploy-online-signing
    COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-garage-deploy-online-signing $<TARGET_FILE:garage-deploy>
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(garage-deploy-online-signing PROPERTIES PASS_REGULAR_EXPRESSION "Provided push credentials are missing required components to sign Targets metadata")

    # Abort if destination server is unavailable.
    add_test(NAME garage-deploy-upload-failed
    COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-garage-deploy-upload-failed $<TARGET_FILE:garage-deploy>
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(garage-deploy-upload-failed PROPERTIES PASS_REGULAR_EXPRESSION "Upload to treehub failed")

    # Abort if commit is not present in source server.
    add_test(NAME garage-deploy-missing-commit
    COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-missing-commit $<TARGET_FILE:garage-deploy>
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    # Abort when given bogus fetch credentials.
    add_test(NAME garage-deploy-missing-fetch-credentials
    COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-garage-deploy-missing-fetch-credentials $<TARGET_FILE:garage-deploy>
    ${SOTA_PACKED_CREDENTIALS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    # Abort when given bogus push credentials.
    add_test(NAME garage-deploy-missing-push-credentials
    COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-garage-deploy-missing-push-credentials $<TARGET_FILE:garage-deploy>
    ${SOTA_PACKED_CREDENTIALS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(garage-deploy-missing-push-credentials PROPERTIES LABELS "credentials")

    if(SOTA_PACKED_CREDENTIALS)
        # Use garage-sign to offline sign targets for destination repository.
        # Remove local tuf repo generated by garage-sign after use.
        add_test(NAME garage-deploy-offline-signing
            COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-garage-deploy-offline-signing $<TARGET_FILE:garage-deploy>
            ${SOTA_PACKED_CREDENTIALS} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        set_tests_properties(garage-deploy-offline-signing PROPERTIES PASS_REGULAR_EXPRESSION "Success" LABELS "credentials")
    endif(SOTA_PACKED_CREDENTIALS)

endif (BUILD_SOTA_TOOLS)

aktualizr_source_file_checks(${GARAGE_PUSH_SRCS} ${GARAGE_CHECK_SRCS} ${GARAGE_DEPLOY_SRCS} ${SOTA_TOOLS_LIB_SRC} ${ALL_SOTA_TOOLS_HEADERS} ${TEST_SOURCES})

# vim: set tabstop=4 shiftwidth=4 expandtab:
