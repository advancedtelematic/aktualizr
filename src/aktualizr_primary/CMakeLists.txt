add_executable(aktualizr main.cc)
target_link_libraries(aktualizr aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr gcov)
endif(BUILD_WITH_CODE_COVERAGE)

aktualizr_source_file_checks(main.cc)

install(TARGETS aktualizr RUNTIME DESTINATION bin COMPONENT aktualizr)

# vim: set tabstop=4 shiftwidth=4 expandtab:
