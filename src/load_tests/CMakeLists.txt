set(LOAD_TESTS_SRC main.cc
                   check.h check.cc
                   provision.h provision.cc
                   executor.h
                   context.h context.cc
                   stats.cc stats.h
                   sslinit.h sslinit.cc)

if (BUILD_LOAD_TESTS)

    add_executable(ota-load-tests ${LOAD_TESTS_SRC})


    add_dependencies(ota-load-tests aktualizr hdr_histogram)

    target_include_directories(ota-load-tests PUBLIC
            ${PROJECT_SOURCE_DIR}/third_party/HdrHistogram_c/src)

    target_link_libraries(ota-load-tests
            aktualizr_static_lib
            hdr_histogram_static
            ${Boost_LIBRARIES}
            ${OPENSSL_LIBRARIES}
            ${sodium_LIBRARY_RELEASE}
            ${CMAKE_THREAD_LIBS_INIT}
            ${CURL_LIBRARIES}
            ${GLIB2_LIBRARIES}
            ${LibArchive_LIBRARIES}
            ${LIBOSTREE_LIBRARIES}
            ${LIBP11_LIBRARIES}
            ${SQLITE3_LIBRARIES}
            ${SYSTEMD_LIBRARY}
            ${LIBDPKG_LIBRARIES})

    if(BUILD_WITH_CODE_COVERAGE)
        target_link_libraries(ota-load-tests gcov)
    endif(BUILD_WITH_CODE_COVERAGE)

    install(TARGETS ota-load-tests
            COMPONENT aktualizr
            RUNTIME DESTINATION bin)
endif (BUILD_LOAD_TESTS)

aktualizr_source_file_checks(${LOAD_TESTS_SRC})