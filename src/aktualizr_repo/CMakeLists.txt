
set(AKTUALIZR_REPO_SRC repo.cc director_repo.cc image_repo.cc uptane_repo.cc)
set(AKTUALIZR_REPO_HDR repo.h director_repo.h image_repo.h uptane_repo.h)

set(AKTUALIZR_REPO_LIBS
    aktualizr_static_lib
    ${AKTUALIZR_EXTERNAL_LIBS}
)
add_library(aktualizr_repo_lib ${AKTUALIZR_REPO_SRC})
add_executable(aktualizr-repo main.cc ${AKTUALIZR_REPO_SRC})
target_link_libraries(aktualizr-repo ${AKTUALIZR_REPO_LIBS})

install(TARGETS aktualizr-repo
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

aktualizr_source_file_checks(${AKTUALIZR_REPO_SRC} ${AKTUALIZR_REPO_HDR})
list(APPEND TEST_LIBS ${AKTUALIZR_REPO_LIBS} aktualizr_repo_lib)
add_aktualizr_test(NAME aktualizr_repo SOURCES repo_test.cc PROJECT_WORKING_DIRECTORY ARGS $<TARGET_FILE:aktualizr-repo>)
target_link_libraries(t_aktualizr_repo ${TEST_LIBS} aktualizr_repo_lib)

aktualizr_source_file_checks(${TEST_SOURCES} main.cc)

# vim: set tabstop=4 shiftwidth=4 expandtab:
