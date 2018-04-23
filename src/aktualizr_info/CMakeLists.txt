
set(AKTUALIZR_INFO_SRC main.cc )

add_executable(aktualizr-info ${AKTUALIZR_INFO_SRC})
target_link_libraries(aktualizr-info aktualizr_static_lib
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${sodium_LIBRARY_RELEASE}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CURL_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${LibArchive_LIBRARIES}
  ${LIBOSTREE_LIBRARIES}
  ${SQLITE3_LIBRARIES})


if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr-info gcov)
endif(BUILD_WITH_CODE_COVERAGE)

install(TARGETS aktualizr-info
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

aktualizr_source_file_checks(${AKTUALIZR_INFO_SRC})

# vim: set tabstop=4 shiftwidth=4 expandtab:
