set(AKTUALIZR_INFO_SRC main.cc aktualizr_info_config.cc)

set(AKTUALIZR_INFO_HEADERS aktualizr_info_config.h)

add_executable(aktualizr-info ${AKTUALIZR_INFO_SRC})
target_link_libraries(aktualizr-info aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})


install(TARGETS aktualizr-info
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

add_aktualizr_test(NAME aktualizr_info_config
                   SOURCES aktualizr_info_config.cc aktualizr_info_config_test.cc PROJECT_WORKING_DIRECTORY)
# Print device ID
# Print primary ECU serial
# Print primary ECU hardware ID
# Print provisioning status
# Print whether metadata has been fetched from the server
add_test(NAME aktualizr-info-noarguments
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-noarguments PROPERTIES
    PASS_REGULAR_EXPRESSION "Device ID: new-kartoffelpuffer-706
Primary ecu serial ID: cd51a49a-25d9-438e-93fa-c0dde6bfcec9
Primary ecu hardware ID: master.*Provisioned on server: yes
Fetched metadata: yes")

# Print secondary ECU serials
# Print secondary ECU hardware IDs
# Print secondary ECUs no longer accessible (removed)
# Print secondary ECUs registered after provisioning (not registered yet)
add_test(NAME aktualizr-info-secondary
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-secondary PROPERTIES
  PASS_REGULAR_EXPRESSION "serial ID: d4dcae5d-39ab-4fed-99a6-97fd36c87c7b\n   hardware ID: secondary.*Removed or not registered ecus:
   '680df471-3096-4064-b2ab-2adc9690157f' with hardware_id 'not-registered' not registered yet
   '84bd6f0b-a1d2-4458-a061-7a406671c70a' with hardware_id 'removed' has been removed from config")

# Print root metadata from images repository
add_test(NAME aktualizr-info-images-root
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --images-root WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-images-root PROPERTIES
    PASS_REGULAR_EXPRESSION "image root.json content:\n\n\\{.*\\}")

# Print root metadata from director repository
add_test(NAME aktualizr-info-director-root
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --director-root WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-director-root PROPERTIES
    PASS_REGULAR_EXPRESSION "director root.json content:\n\n\\{.*\\}")

# Print targets metadata from director repository
add_test(NAME aktualizr-info-director-target
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --director-target WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-director-target PROPERTIES
    PASS_REGULAR_EXPRESSION "director targets.json content:\n\n\\{.*\\}")

# Print targets metadata from images repository
add_test(NAME aktualizr-info-images-target
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --images-target WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-images-target PROPERTIES
    PASS_REGULAR_EXPRESSION "image targets.json content:\n\n\\{.*\\}")

# Print primary ECU keys
add_test(NAME aktualizr-info-ECU-keys
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --ecu-keys WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-ECU-keys PROPERTIES
    PASS_REGULAR_EXPRESSION "Public key:\n-----BEGIN PUBLIC KEY-----.*-----END PUBLIC KEY-----
\nPrivate key:
-----BEGIN RSA PRIVATE KEY-----.*-----END RSA PRIVATE KEY-----")

# Print TLS credentials
add_test(NAME aktualizr-info-tls-creds
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --tls-creds WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-tls-creds PROPERTIES
    PASS_REGULAR_EXPRESSION "Root CA certificate:
-----BEGIN CERTIFICATE-----.*Client certificate:
-----BEGIN CERTIFICATE-----.*Client private key:
-----BEGIN PRIVATE KEY-----.*-----END PRIVATE KEY-----")


aktualizr_source_file_checks(${AKTUALIZR_INFO_SRC}
                             ${AKTUALIZR_INFO_HEADERS}
                             ${TEST_SOURCES})

# vim: set tabstop=4 shiftwidth=4 expandtab:
