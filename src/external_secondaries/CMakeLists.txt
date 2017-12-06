# set headers used for clang format
set(ECU_INTERFACE_HEADERS ecuinterface.h)

set(MAIN_ECU_INTERFACE_SRC ecuinterface.cc)

add_executable(example-interface ${MAIN_ECU_INTERFACE_SRC} example_interface.cc)
target_link_libraries(example-interface ${Boost_LIBRARIES})

add_executable(aktualizr-validate-secondary-interface validate_secondary_interface_test.cc)
set_property(TARGET aktualizr-validate-secondary-interface PROPERTY CXX_STANDARD 11)
target_link_libraries(aktualizr-validate-secondary-interface ${Boost_LIBRARIES} gtest)

aktualizr_source_file_checks(${MAIN_ECU_INTERFACE_SRC} ${ECU_INTERFACE_HEADERS} example_interface.cc)


# vim: set tabstop=4 shiftwidth=4 expandtab:
