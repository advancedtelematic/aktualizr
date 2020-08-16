set(AKTUALIZR_LITE_SRC main.cc helpers.cc)
set(AKTUALIZR_LITE_HEADERS helpers.h)

if(BUILD_OSTREE)
add_executable(aktualizr-lite ${AKTUALIZR_LITE_SRC})
target_link_libraries(aktualizr-lite aktualizr_lib)

install(TARGETS aktualizr-lite RUNTIME DESTINATION bin COMPONENT aktualizr-lite)

set(TEST_SOURCES test_lite.sh)

add_dependencies(build_tests aktualizr-lite)

add_test(test_aktualizr-lite
    ${CMAKE_CURRENT_SOURCE_DIR}/test_lite.sh
        ${CMAKE_BINARY_DIR}/src/aktualizr_lite/aktualizr-lite
        ${CMAKE_BINARY_DIR}/src/uptane_generator/uptane-generator
        ${PROJECT_SOURCE_DIR}/tests
        ${RUN_VALGRIND}
)
add_library(t_lite-mock SHARED ostree_mock.cc)
add_aktualizr_test(NAME lite-helpers SOURCES helpers.cc helpers_test.cc
                   ARGS ${PROJECT_BINARY_DIR}/ostree_repo)
add_dependencies(t_lite-helpers t_lite-mock)
set_tests_properties(test_lite-helpers PROPERTIES
        ENVIRONMENT LD_PRELOAD=$<TARGET_FILE:t_lite-mock> LABELS "noptest")

# Check the --help option works.
add_test(NAME aktualizr-lite-option-help
         COMMAND aktualizr-lite --help)

# Report version.
add_test(NAME aktualizr-lite-option-version
         COMMAND aktualizr-lite --version)
set_tests_properties(aktualizr-lite-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current aktualizr-lite version is: ${AKTUALIZR_VERSION}")

endif(BUILD_OSTREE)

aktualizr_source_file_checks(main.cc ${AKTUALIZR_LITE_SRC} ${AKTUALIZR_LITE_HEADERS} helpers_test.cc ostree_mock.cc)
# vim: set tabstop=4 shiftwidth=4 expandtab:
