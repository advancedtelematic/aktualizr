set(AKTUALIZR_SECONDARY_SRC main.cc )

add_executable(aktualizr-secondary ${AKTUALIZR_SECONDARY_SRC})
target_link_libraries(aktualizr-secondary aktualizr_static_lib
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${sodium_LIBRARY_RELEASE}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CURL_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${LibArchive_LIBRARIES}
  gcov)

install(TARGETS aktualizr-secondary
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

aktualizr_source_file_checks(${AKTUALIZR_SECONDARY_SRC})

# vim: set tabstop=4 shiftwidth=4 expandtab:
