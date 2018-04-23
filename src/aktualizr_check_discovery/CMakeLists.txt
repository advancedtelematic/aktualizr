
set(AKTUALIZR_CHECK_DISCOVERY_SRC main.cc )

add_executable(aktualizr-check-discovery ${AKTUALIZR_CHECK_DISCOVERY_SRC})
target_link_libraries(aktualizr-check-discovery aktualizr_static_lib
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${sodium_LIBRARY_RELEASE}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CURL_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${LibArchive_LIBRARIES}
  ${SQLITE3_LIBRARIES}
  ${LIBOSTREE_LIBRARIES}
  ${SYSTEMD_LIBRARY})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr-check-discovery gcov)
endif(BUILD_WITH_CODE_COVERAGE)

install(TARGETS aktualizr-check-discovery
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

aktualizr_source_file_checks(${AKTUALIZR_CHECK_DISCOVERY_SRC})

# vim: set tabstop=4 shiftwidth=4 expandtab:
