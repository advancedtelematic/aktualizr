# set headers used for clang format
set(ECU_INTERFACE_HEADERS ecuinterface.h)

set(MAIN_ECU_INTERFACE_SRC ecuinterface.cc)

add_executable(myecuinterface ${MAIN_ECU_INTERFACE_SRC} dummy.cc)
target_link_libraries(myecuinterface ${Boost_LIBRARIES})

aktualizr_source_file_checks(${MAIN_ECU_INTERFACE_SRC} ${ECU_INTERFACE_HEADERS} dummy.cc)

# vim: set tabstop=4 shiftwidth=4 expandtab:
