set(LOAD_TESTS_SRC main.cc
                   check.h check.cc
                   provision.h provision.cc
                   executor.h
                   context.h context.cc
                   stats.cc stats.h
                   sslinit.h sslinit.cc executor.cc)

set(LT_TREEHUB_SRC treehub.cc treehub.h)

if (BUILD_LOAD_TESTS)

    add_executable(ota-load-tests ${LOAD_TESTS_SRC})

    if (BUILD_OSTREE)
        target_sources(ota-load-tests PRIVATE ${LT_TREEHUB_SRC})
    endif (BUILD_OSTREE)

    add_dependencies(ota-load-tests aktualizr hdr_histogram)

    target_include_directories(ota-load-tests PRIVATE ${PROJECT_SOURCE_DIR}/third_party/HdrHistogram_c/src)

    target_link_libraries(ota-load-tests aktualizr_lib hdr_histogram_static)

    install(TARGETS ota-load-tests COMPONENT aktualizr RUNTIME DESTINATION bin)
endif (BUILD_LOAD_TESTS)

aktualizr_source_file_checks(${LOAD_TESTS_SRC} ${LT_TREEHUB_SRC})
