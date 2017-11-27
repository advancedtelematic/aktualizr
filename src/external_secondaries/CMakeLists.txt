# set headers used for clang format
set(ECU_INTERFACE_HEADERS ecuinterface.h)

set(MAIN_ECU_INTERFACE_SRC ecuinterface.cc)

add_executable(myecuinterface ${MAIN_ECU_INTERFACE_SRC} dummy.cc)
target_link_libraries(myecuinterface ${Boost_LIBRARIES})

add_executable(aktualizr-validate-secondary-interface validate_secondary_interface_test.cc)
set_property(TARGET aktualizr-validate-secondary-interface PROPERTY CXX_STANDARD 11)
target_link_libraries(aktualizr-validate-secondary-interface ${Boost_LIBRARIES} gtest)

aktualizr_source_file_checks(${MAIN_ECU_INTERFACE_SRC} ${ECU_INTERFACE_HEADERS} dummy.cc)


# vim: set tabstop=4 shiftwidth=4 expandtab:
