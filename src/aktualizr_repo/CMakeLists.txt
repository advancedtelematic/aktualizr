
set(AKTUALIZR_REPO_SRC main.cc repo.cc)
set(AKTUALIZR_REPO_HDR repo.h)


add_executable(aktualizr-repo ${AKTUALIZR_REPO_SRC})
target_link_libraries(aktualizr-repo aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr-repo gcov)
endif(BUILD_WITH_CODE_COVERAGE)

install(TARGETS aktualizr-repo
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

aktualizr_source_file_checks(${AKTUALIZR_REPO_SRC} ${AKTUALIZR_REPO_HDR})

# vim: set tabstop=4 shiftwidth=4 expandtab:
