# set the name of the executable
add_executable(aktualizr-cert-provider main.cc
    $<TARGET_OBJECTS:bootstrap>
    $<TARGET_OBJECTS:config>
    $<TARGET_OBJECTS:crypto>
    $<TARGET_OBJECTS:http>
    $<TARGET_OBJECTS:jsoncpp>
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:primary_config>
    $<TARGET_OBJECTS:utilities>)

set_source_files_properties(main.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

target_link_libraries(aktualizr-cert-provider
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_SYSTEM_LIBRARIES}
    ${Boost_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${LibArchive_LIBRARIES}
    ${LIBP11_LIBRARIES}
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${sodium_LIBRARY_RELEASE}
    )

aktualizr_source_file_checks(main.cc)

add_dependencies(build_tests aktualizr-cert-provider)

install(TARGETS aktualizr-cert-provider RUNTIME DESTINATION bin COMPONENT garage_deploy)

add_aktualizr_test(NAME aktualizr_cert_provider SOURCES cert_provider_test.cc PROJECT_WORKING_DIRECTORY ARGS $<TARGET_FILE:aktualizr-cert-provider>)

if (SOTA_PACKED_CREDENTIALS)
    add_aktualizr_test(NAME aktualizr_cert_provider_shared_cred SOURCES cert_provider_shared_cred_test.cc
        PROJECT_WORKING_DIRECTORY ARGS $<TARGET_FILE:aktualizr-cert-provider> ${SOTA_PACKED_CREDENTIALS})
    set_tests_properties(test_aktualizr_cert_provider_shared_cred PROPERTIES LABELS "credentials")
endif(SOTA_PACKED_CREDENTIALS)

# Check the --help option works.
add_test(NAME aktualizr-cert-provider-option-help
         COMMAND aktualizr-cert-provider --help)

# Report version.
add_test(NAME aktualizr-cert-provider-option-version
         COMMAND aktualizr-cert-provider --version)
set_tests_properties(aktualizr-cert-provider-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current aktualizr-cert-provider version is: ${AKTUALIZR_VERSION}")

aktualizr_source_file_checks(${AKTUALIZR_CERT_PROVIDER_SRC}
                             ${AKTUALIZR_CERT_HEADERS}
                             cert_provider_shared_cred_test.cc
                             cert_provider_test.cc
                             cert_provider_test.h)

# vim: set tabstop=4 shiftwidth=4 expandtab:
