set(AKTUALIZR_INFO_SRC main.cc aktualizr_info_config.cc)

set(AKTUALIZR_INFO_HEADERS aktualizr_info_config.h)

add_executable(aktualizr-info ${AKTUALIZR_INFO_SRC})
target_link_libraries(aktualizr-info aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})


if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr-info gcov)
endif(BUILD_WITH_CODE_COVERAGE)

install(TARGETS aktualizr-info
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

add_aktualizr_test(NAME aktualizr_info_config
                   SOURCES aktualizr_info_config.cc aktualizr_info_config_test.cc PROJECT_WORKING_DIRECTORY)

add_test(NAME aktualizr-info-noarguments
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-noarguments PROPERTIES
    PASS_REGULAR_EXPRESSION "Device ID: new-kartoffelpuffer-706\nPrimary ecu serial ID: cd51a49a-25d9-438e-93fa-c0dde6bfcec9")

add_test(NAME aktualizr-info-images-root
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --images-root WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-images-root PROPERTIES
    PASS_REGULAR_EXPRESSION "image root.json content:\n\n\\{.*\\}")

add_test(NAME aktualizr-info-director-root
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --director-root WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-director-root PROPERTIES
    PASS_REGULAR_EXPRESSION "director root.json content:\n\n\\{.*\\}")

add_test(NAME aktualizr-info-director-target
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --director-target WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-director-target PROPERTIES
    PASS_REGULAR_EXPRESSION "director targets.json content:\n\n\\{.*\\}")

add_test(NAME aktualizr-info-images-target
    COMMAND ${PROJECT_SOURCE_DIR}/tests/run_aktualizr_info_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/aktualizr-info --images-target WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/)
set_tests_properties(aktualizr-info-images-target PROPERTIES
    PASS_REGULAR_EXPRESSION "image targets.json content:\n\n\\{.*\\}")

set(AKTUALIZR_INFO_ALL_CHECKS ${AKTUALIZR_INFO_SRC} ${AKTUALIZR_INFO_HEADERS} ${TEST_SOURCES})
list(REMOVE_DUPLICATES AKTUALIZR_INFO_ALL_CHECKS)
aktualizr_source_file_checks(${AKTUALIZR_INFO_ALL_CHECKS})
# vim: set tabstop=4 shiftwidth=4 expandtab:
