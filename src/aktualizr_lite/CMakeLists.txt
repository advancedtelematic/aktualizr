if(BUILD_OSTREE)
add_executable(aktualizr-lite main.cc version.h)
target_link_libraries(aktualizr-lite aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

install(TARGETS aktualizr-lite RUNTIME DESTINATION bin COMPONENT aktualizr-lite)

set(TEST_SOURCES test_lite.sh)

add_dependencies(build_tests aktualizr-lite)

add_test(test_aktualizr-lite
    ${CMAKE_CURRENT_SOURCE_DIR}/test_lite.sh
        ${CMAKE_BINARY_DIR}/src/aktualizr_lite/aktualizr-lite
        ${CMAKE_BINARY_DIR}/src/aktualizr_repo/aktualizr-repo
        ${PROJECT_SOURCE_DIR}/tests
        ${RUN_VALGRIND}
)
add_library(t_lite-mock SHARED ostree_mock.cc)

endif(BUILD_OSTREE)

add_aktualizr_test(NAME lite-version SOURCES version_test.cc)

aktualizr_source_file_checks(main.cc version.h version_test.cc ostree_mock.cc)
# vim: set tabstop=4 shiftwidth=4 expandtab:
