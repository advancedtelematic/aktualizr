# set headers used for clang format
set(ECU_INTERFACE_HEADERS ecuinterface.h)

set(MAIN_ECU_INTERFACE_SRC interface_main.cc make_ecu.cc)

add_executable(example-interface ${MAIN_ECU_INTERFACE_SRC} example_flasher.cc)
target_link_libraries(example-interface ${Boost_SYSTEM_LIBRARIES} aktualizr_static_lib ${Boost_LIBRARIES})
target_compile_options(example-interface PUBLIC -DECUFLASHER_EXAMPLE)

add_executable(aktualizr-validate-secondary-interface validate_secondary_interface_test.cc)
set_property(TARGET aktualizr-validate-secondary-interface PROPERTY CXX_STANDARD 11)
target_link_libraries(aktualizr-validate-secondary-interface ${Boost_LIBRARIES} gtest)

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(example-interface gcov)
    target_link_libraries(aktualizr-validate-secondary-interface gcov)
endif(BUILD_WITH_CODE_COVERAGE)

aktualizr_source_file_checks(${MAIN_ECU_INTERFACE_SRC} ${ECU_INTERFACE_HEADERS} example_flasher.cc validate_secondary_interface_test.cc)

install(TARGETS example-interface RUNTIME DESTINATION bin)


if(BUILD_ISOTP)
    set(ISOTP_PATH_PREFIX ../../partial/extern/isotp-c/src)
    set(BITFIELD_PATH_PREFIX ../../partial/extern/isotp-c/deps/bitfield-c/src)
    set(ISOTP_SRCS ${ISOTP_PATH_PREFIX}/isotp/isotp.c
        ${ISOTP_PATH_PREFIX}/isotp/send.c
        ${ISOTP_PATH_PREFIX}/isotp/receive.c
        ${BITFIELD_PATH_PREFIX}/bitfield/8byte.c
        ${BITFIELD_PATH_PREFIX}/bitfield/bitarray.c
        ${BITFIELD_PATH_PREFIX}/bitfield/bitfield.c)

    add_executable(isotp-test-interface ${MAIN_ECU_INTERFACE_SRC} test_isotp_interface.cc isotp_allocate.cc isotp_protocol.cc ${ISOTP_SRCS})
    target_link_libraries(isotp-test-interface aktualizr_static_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
    target_compile_options(isotp-test-interface PUBLIC -DECUFLASHER_TEST_ISOTP)
    target_include_directories(isotp-test-interface PUBLIC ${ISOTP_PATH_PREFIX} ${BITFIELD_PATH_PREFIX})

    if(BUILD_WITH_CODE_COVERAGE)
        target_link_libraries(isotp-test-interface gcov)
    endif(BUILD_WITH_CODE_COVERAGE)

    aktualizr_source_file_checks(test_isotp_interface.cc isotp_allocate.cc isotp_protocol.cc)

    install(TARGETS isotp-test-interface RUNTIME DESTINATION bin)
endif(BUILD_ISOTP)

# vim: set tabstop=4 shiftwidth=4 expandtab:
