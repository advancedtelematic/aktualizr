# set the name of the executable
add_executable(aktualizr_implicit_writer main.cc)

target_link_libraries(aktualizr_implicit_writer aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${sodium_LIBRARY_RELEASE}
        ${SQLITE3_LIBRARIES}
        ${LibArchive_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr_implicit_writer gcov)
endif(BUILD_WITH_CODE_COVERAGE)

aktualizr_source_file_checks(main.cc)

# The tests run aktualizr_implicit_writer
add_dependencies(build_tests aktualizr_implicit_writer)

install(TARGETS aktualizr_implicit_writer RUNTIME DESTINATION bin)

# vim: set tabstop=4 shiftwidth=4 expandtab:
