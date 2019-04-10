
set(AKTUALIZR_CHECK_DISCOVERY_SRC main.cc )

add_executable(aktualizr-check-discovery ${AKTUALIZR_CHECK_DISCOVERY_SRC})
target_link_libraries(aktualizr-check-discovery aktualizr-posix ${AKTUALIZR_EXTERNAL_LIBS})

install(TARGETS aktualizr-check-discovery
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

target_include_directories(aktualizr-check-discovery PUBLIC ${PROJECT_SOURCE_DIR}/src/libaktualizr-posix)

aktualizr_source_file_checks(${AKTUALIZR_CHECK_DISCOVERY_SRC})

# vim: set tabstop=4 shiftwidth=4 expandtab:
