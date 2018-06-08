include_directories("${PROJECT_SOURCE_DIR}/src/libaktualizr/third_party/jsoncpp")
set (SOTA_TOOLS_LIB_SRC
  authenticate.cc
  deploy.cc
  oauth2.cc
  ostree_hash.cc
  ostree_ref.cc
  ostree_dir_repo.cc
  ostree_http_repo.cc
  ostree_object.cc
  rate_controller.cc
  request_pool.cc
  server_credentials.cc
  treehub_server.cc
)

if (BUILD_SOTA_TOOLS)
    add_library(sota_tools_static_lib STATIC ${SOTA_TOOLS_LIB_SRC})
    target_include_directories(sota_tools_static_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR} ${GLIB2_INCLUDE_DIRS})
endif (BUILD_SOTA_TOOLS)
##### garage-push targets

set (GARAGE_PUSH_SRCS
  garage_push.cc
)

set (SOTA_TOOLS_EXTERNAL_LIBS
  ${Boost_SYSTEM_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${LibArchive_LIBRARIES}
  ${CURL_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${sodium_LIBRARY_RELEASE}
  ${GLIB2_LIBRARIES})

if (BUILD_SOTA_TOOLS)
    add_executable(garage-push ${GARAGE_PUSH_SRCS})

    target_link_libraries(garage-push sota_tools_static_lib aktualizr_static_lib ${SOTA_TOOLS_EXTERNAL_LIBS})


    if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(garage-push gcov)
    endif(BUILD_WITH_CODE_COVERAGE)

    install(TARGETS garage-push RUNTIME DESTINATION bin COMPONENT garage_deploy)
endif (BUILD_SOTA_TOOLS)

##### garage-check targets
set (GARAGE_CHECK_SRCS
  garage_check.cc
)

if (BUILD_SOTA_TOOLS)
    add_executable(garage-check ${GARAGE_CHECK_SRCS})

    target_link_libraries(garage-check sota_tools_static_lib aktualizr_static_lib ${SOTA_TOOLS_EXTERNAL_LIBS})

    if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(garage-check gcov)
    endif(BUILD_WITH_CODE_COVERAGE)

    install(TARGETS garage-check RUNTIME DESTINATION bin COMPONENT garage_deploy)
endif (BUILD_SOTA_TOOLS)

##### garage-deploy targets

set (GARAGE_DEPLOY_SRCS
  garage_deploy.cc
)

if (BUILD_SOTA_TOOLS)
    add_executable(garage-deploy ${GARAGE_DEPLOY_SRCS})
    target_link_libraries(garage-deploy sota_tools_static_lib aktualizr_static_lib ${SOTA_TOOLS_EXTERNAL_LIBS})

    if(BUILD_WITH_CODE_COVERAGE)
        target_link_libraries(garage-deploy gcov)
    endif(BUILD_WITH_CODE_COVERAGE)

    add_definitions(-DGARAGE_DEPLOY_VERSION="${AKTUALIZR_VERSION}")

    add_dependencies(build_tests garage-deploy)

    install(TARGETS garage-deploy RUNTIME DESTINATION bin COMPONENT garage_deploy)


    ### garage-sign targets

    if(GARAGE_SIGN_VERSION)
    set(GARAGE_SIGN_VERSION_ARG "--name" ${GARAGE_SIGN_VERSION})
    endif(GARAGE_SIGN_VERSION)
    if(GARAGE_SIGN_SHA256)
    set(GARAGE_SIGN_SHA256_ARG "--sha256" ${GARAGE_SIGN_SHA256})
    endif(GARAGE_SIGN_SHA256)
    add_custom_target(garage-sign
                    COMMAND ${PROJECT_SOURCE_DIR}/scripts/get_garage_sign.py
                    --output ${CMAKE_CURRENT_BINARY_DIR}
                    ${GARAGE_SIGN_VERSION_ARG} ${GARAGE_SIGN_SHA256_ARG})
    add_dependencies(garage-deploy garage-sign)

    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/garage-sign/bin/garage-sign DESTINATION bin COMPONENT garage_deploy)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/garage-sign/lib DESTINATION . COMPONENT garage_deploy)
endif (BUILD_SOTA_TOOLS)

##### clang-format

# For clang-format
set(ALL_SOTA_TOOLS_HEADERS
  accumulator.h
  authenticate.h
  deploy.h
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
  treehub_server.h
)

if (NOT BUILD_SOTA_TOOLS)
    set (TEST_SOURCES
        authenticate_test.cc
        ostree_hash_test.cc
        rate_controller_test.cc
    )
endif(NOT BUILD_SOTA_TOOLS)

if (BUILD_SOTA_TOOLS)
    add_aktualizr_test(NAME sota_tools_auth_test SOURCES authenticate_test.cc
                            PROJECT_WORKING_DIRECTORY NO_VALGRIND)
        target_link_libraries(t_sota_tools_auth_test sota_tools_static_lib aktualizr_static_lib ${TEST_LIBS})

    add_aktualizr_test(NAME ostree_hash SOURCES ostree_hash_test.cc)
        target_link_libraries(t_ostree_hash sota_tools_static_lib)
        target_include_directories(t_ostree_hash
                                PUBLIC ${PROJECT_SOURCE_DIR}/src/sota_tools ${GLIB2_INCLUDE_DIRS})

    add_aktualizr_test(NAME rate_controller SOURCES rate_controller_test.cc)
    target_link_libraries(t_rate_controller
                        sota_tools_static_lib
                        ${Boost_LIBRARIES})
    # Setup coverage for sota_tools_static_lib
    if(BUILD_WITH_CODE_COVERAGE)
        target_compile_options(sota_tools_static_lib PUBLIC -fprofile-arcs -ftest-coverage)
    endif(BUILD_WITH_CODE_COVERAGE)

    # Check the --help option works
    add_test(NAME option-help
        COMMAND garage-push --help)

    # Invalid arguments result in a failure
    add_test(NAME bad-option
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-bad-option $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Detect that the thing we passed in to --repo is not a repository
    add_test(NAME invalid-repo
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-invalid-repo $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Detect a --repo that isn't in 'archive-z2' mode
    add_test(NAME bare-mode-repo
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-bare-mode-repo $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    # Detect that the ref we passed in to --ref is not present
    add_test(NAME missing-ref
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-missing-ref $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    # Handle a missing credentials.zip
    add_test(NAME missing-credentials
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-missing-credentials $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    add_test(NAME check_not_expired_targets 
        COMMAND ${PROJECT_SOURCE_DIR}/tests/run_expired_test.sh 2019-11-17T23:58:40Z ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_test(NAME check_expired_targets 
        COMMAND ${PROJECT_SOURCE_DIR}/tests/run_expired_test.sh 2017-11-17T23:58:40Z ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(check_expired_targets  PROPERTIES WILL_FAIL TRUE)

        # Handle passing a random file as credentials.zip
    add_test(NAME invalid-credentials
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-invalid-credentials $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    add_test(NAME cacert-not-found
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-cacert-not-found $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    add_test(NAME server-500
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-server-500 $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/sota_tools)

    add_test(NAME server-500-every-10-request
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-server-error_every_10 $<TARGET_FILE:garage-push> 500
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/sota_tools)

    add_test(NAME server-409-every-10-request
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-server-error_every_10 $<TARGET_FILE:garage-push> 409
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/sota_tools)

    add_test(NAME server-500_after_20
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-server-500_after_20 $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/sota_tools)

    add_test(NAME auth-plus-failure
        COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-auth-plus-failure $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)

    if(SOTA_PACKED_CREDENTIALS)
        add_test(NAME dry-run
            COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-dry-run $<TARGET_FILE:garage-push>
            ${SOTA_PACKED_CREDENTIALS} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
        set_tests_properties(dry-run PROPERTIES LABELS "credentials")

        add_test(NAME auth-plus-happy-case
            COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-auth-plus-happy $<TARGET_FILE:garage-push>
            ${SOTA_PACKED_CREDENTIALS} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
        set_tests_properties(auth-plus-happy-case PROPERTIES LABELS "credentials")

        add_test(NAME verbose-logging
            COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-verbose-logging $<TARGET_FILE:garage-push>
            ${SOTA_PACKED_CREDENTIALS} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
        set_tests_properties(verbose-logging PROPERTIES LABELS "credentials")

        if(STRACE)
            add_test(NAME cacert-used
                COMMAND ${PROJECT_SOURCE_DIR}/tests/sota_tools/test-cacert-used $<TARGET_FILE:garage-push>
                ${SOTA_PACKED_CREDENTIALS} ${STRACE} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
            set_tests_properties(cacert-used PROPERTIES LABELS "credentials")
        endif(STRACE)
    endif(SOTA_PACKED_CREDENTIALS)

    # garage-deploy tests
    add_test(NAME garage-deploy-option-help
        COMMAND garage-deploy --help)

    add_test(NAME garage-deploy-nonexistent-option
        COMMAND garage-deploy --test )
    set_tests_properties(garage-deploy-nonexistent-option PROPERTIES WILL_FAIL TRUE)
endif (BUILD_SOTA_TOOLS)
aktualizr_source_file_checks(${GARAGE_PUSH_SRCS} ${GARAGE_CHECK_SRCS} ${GARAGE_DEPLOY_SRCS} ${SOTA_TOOLS_LIB_SRC} ${ALL_SOTA_TOOLS_HEADERS} ${TEST_SOURCES})


# vim: set tabstop=2 shiftwidth=2 expandtab:
