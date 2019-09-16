add_executable(hmi-stub main.cc)
target_link_libraries(hmi-stub aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

aktualizr_source_file_checks(main.cc)

install(TARGETS hmi-stub RUNTIME DESTINATION bin COMPONENT hmi-stub)

# vim: set tabstop=4 shiftwidth=4 expandtab:
