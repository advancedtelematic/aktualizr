# set headers used for clang format
set(ECU_INTERFACE_HEADERS ecuinterface.h make_ecu.h)

set(MAIN_ECU_INTERFACE_SRC interface_main.cc make_ecu.cc)

add_executable(example-interface ${MAIN_ECU_INTERFACE_SRC} example_flasher.cc)
target_link_libraries(example-interface
                      aktualizr_static_lib
                      ${AKTUALIZR_EXTERNAL_LIBS})
target_compile_options(example-interface PUBLIC -DECUFLASHER_EXAMPLE)

add_executable(aktualizr-validate-secondary-interface validate_secondary_interface_test.cc)
target_link_libraries(aktualizr-validate-secondary-interface ${Boost_LIBRARIES} gtest)

aktualizr_source_file_checks(${MAIN_ECU_INTERFACE_SRC} ${ECU_INTERFACE_HEADERS}
                             example_flasher.cc example_flasher.h validate_secondary_interface_test.cc)

install(TARGETS example-interface RUNTIME DESTINATION bin)


set(ISOTP_PATH_PREFIX ../../partial/extern/isotp-c/src)
set(BITFIELD_PATH_PREFIX ../../partial/extern/isotp-c/deps/bitfield-c/src)
set(ISOTP_SRC ${ISOTP_PATH_PREFIX}/isotp/isotp.c
    ${ISOTP_PATH_PREFIX}/isotp/send.c
    ${ISOTP_PATH_PREFIX}/isotp/receive.c
    ${BITFIELD_PATH_PREFIX}/bitfield/8byte.c
    ${BITFIELD_PATH_PREFIX}/bitfield/bitarray.c
    ${BITFIELD_PATH_PREFIX}/bitfield/bitfield.c)
set(ISOTP_TEST_SRC test_isotp_interface.cc isotp_allocate.cc isotp_protocol.cc )
set(ISOTP_TEST_HEADERS test_isotp_interface.h)
set_source_files_properties(${ISOTP_SRC} PROPERTIES COMPILE_FLAGS "-Wno-sign-conversion -Wno-conversion -Wno-error=unused-parameter")
aktualizr_source_file_checks(${ISOTP_TEST_SRC} ${ISOTP_TEST_HEADERS})

if(BUILD_ISOTP)
    add_executable(isotp-test-interface ${MAIN_ECU_INTERFACE_SRC} ${ISOTP_TEST_SRC} ${ISOTP_SRC})
    target_link_libraries(isotp-test-interface
                          aktualizr_static_lib
                          ${AKTUALIZR_EXTERNAL_LIBS})
    target_compile_options(isotp-test-interface PUBLIC -DECUFLASHER_TEST_ISOTP)
    target_include_directories(isotp-test-interface PUBLIC ${ISOTP_PATH_PREFIX} ${BITFIELD_PATH_PREFIX})

    install(TARGETS isotp-test-interface RUNTIME DESTINATION bin)
endif(BUILD_ISOTP)

add_dependencies(build_tests aktualizr-validate-secondary-interface example-interface)
add_test(NAME validate-secondary-interface
    COMMAND aktualizr-validate-secondary-interface
    --target ${PROJECT_BINARY_DIR}/src/external_secondaries/example-interface
    --firmware ${PROJECT_SOURCE_DIR}/tests/test_data/firmware.txt)


# vim: set tabstop=4 shiftwidth=4 expandtab:
