set(AKTUALIZR_SECONDARY_SRC main.cc)

set(AKTUALIZR_SECONDARY_LIB_SRC
    aktualizr_secondary_config.cc
    )

add_library(aktualizr_secondary_static_lib STATIC ${AKTUALIZR_SECONDARY_LIB_SRC})

add_executable(aktualizr-secondary ${AKTUALIZR_SECONDARY_SRC})
target_link_libraries(aktualizr-secondary
  aktualizr_secondary_static_lib
  aktualizr_static_lib
  socket_activation
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

set(ALL_AKTUALIZR_SECONDARY_HEADERS
    aktualizr_secondary_config.h
    )

aktualizr_source_file_checks(${AKTUALIZR_SECONDARY_SRC} ${AKTUALIZR_SECONDARY_LIB_SRC} ${ALL_AKTUALIZR_SECONDARY_HEADERS})

# vim: set tabstop=4 shiftwidth=4 expandtab:
