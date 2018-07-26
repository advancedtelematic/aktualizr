add_executable(hmi_stub main.cc)
target_link_libraries(hmi_stub aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

aktualizr_source_file_checks(main.cc)

install(TARGETS hmi_stub RUNTIME DESTINATION bin COMPONENT hmi_stub)

# vim: set tabstop=4 shiftwidth=4 expandtab:
