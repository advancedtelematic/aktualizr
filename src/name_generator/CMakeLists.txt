# set the name of the executable
add_executable(aktualizr_name_generator main.cc)

target_link_libraries(aktualizr_name_generator aktualizr_static_lib ${Boost_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr_name_generator gcov)
endif(BUILD_WITH_CODE_COVERAGE)

aktualizr_source_file_checks(main.cc)

install(TARGETS aktualizr_name_generator RUNTIME DESTINATION bin)

# vim: set tabstop=4 shiftwidth=4 expandtab:
