
add_executable(aktualizr-get main.cc get.cc)
target_link_libraries(aktualizr-get aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

install(TARGETS aktualizr-get RUNTIME DESTINATION bin COMPONENT aktualizr-get)

add_aktualizr_test(NAME aktualizr_get
                   SOURCES get.cc get_test.cc
                   PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(main.cc get.cc get.h get_test.cc)

# vim: set tabstop=4 shiftwidth=4 expandtab:
