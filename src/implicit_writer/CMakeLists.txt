# set the name of the executable
add_executable(aktualizr_implicit_writer main.cc)

target_link_libraries(aktualizr_implicit_writer aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${sodium_LIBRARY_RELEASE}
        ${LibArchive_LIBRARIES} gcov)

aktualizr_source_file_checks(main.cc)

install(TARGETS aktualizr_implicit_writer RUNTIME DESTINATION bin)

# vim: set tabstop=4 shiftwidth=4 expandtab:
