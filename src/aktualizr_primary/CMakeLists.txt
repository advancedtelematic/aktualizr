add_executable(aktualizr main.cc)
target_link_libraries(aktualizr aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${sodium_LIBRARY_RELEASE}
        ${LIBOSTREE_LIBRARIES}
        ${LIBP11_LIBRARIES}
        ${SQLITE3_LIBRARIES}
        ${LibArchive_LIBRARIES}
        ${SYSTEMD_LIBRARY}
        ${LIBDPKG_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr gcov)
endif(BUILD_WITH_CODE_COVERAGE)

aktualizr_source_file_checks(main.cc)

install(TARGETS aktualizr RUNTIME DESTINATION bin COMPONENT aktualizr)

# vim: set tabstop=4 shiftwidth=4 expandtab:
