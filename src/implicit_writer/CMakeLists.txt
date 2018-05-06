# set the name of the executable
add_executable(aktualizr_implicit_writer main.cc)

target_link_libraries(aktualizr_implicit_writer aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr_implicit_writer gcov)
endif(BUILD_WITH_CODE_COVERAGE)

aktualizr_source_file_checks(main.cc)

# The tests run aktualizr_implicit_writer
add_dependencies(build_tests aktualizr_implicit_writer)

install(TARGETS aktualizr_implicit_writer RUNTIME DESTINATION bin)


# implicit_writer tests
add_test(NAME implicit-writer-help COMMAND aktualizr_implicit_writer --help)

add_test(NAME implicit-writer-test
    COMMAND ${PROJECT_SOURCE_DIR}/tests/implicit_writer_test.py
    -c ${PROJECT_SOURCE_DIR}/tests/test_data/credentials.zip
    -i ${PROJECT_SOURCE_DIR}/config/sota_implicit_prov.toml
    -o ${CMAKE_CURRENT_BINARY_DIR}/implicit -w ${CMAKE_CURRENT_BINARY_DIR})


# vim: set tabstop=4 shiftwidth=4 expandtab:
