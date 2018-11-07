# set the name of the executable
add_executable(aktualizr-cert-provider main.cc)

set_source_files_properties(main.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

target_link_libraries(aktualizr-cert-provider aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

aktualizr_source_file_checks(main.cc)

add_dependencies(build_tests aktualizr-cert-provider)

install(TARGETS aktualizr-cert-provider RUNTIME DESTINATION bin)

add_test(NAME certprovider-ca COMMAND ${PROJECT_SOURCE_DIR}/tests/run_certprovider_test.sh
        ${PROJECT_BINARY_DIR}/src/cert_provider/aktualizr-cert-provider ${PROJECT_SOURCE_DIR}/tests/test_data/credentials.zip
        ${PROJECT_SOURCE_DIR}/tests/test_data/CAcert.pem ${PROJECT_SOURCE_DIR}/tests/test_data/CApkey.pem)

# vim: set tabstop=4 shiftwidth=4 expandtab:
