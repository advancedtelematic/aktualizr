set(LOAD_TESTS_SRC main.cc
                   check.h check.cc
                   provision.h provision.cc
                   executor.h
                   context.h context.cc
                   stats.cc stats.h
                   sslinit.h sslinit.cc)

if (BUILD_LOAD_TESTS)

    add_executable(ota-load-tests ${LOAD_TESTS_SRC})

    include(ExternalProject)
    ExternalProject_Add(
            hdr_histogram
            URL https://github.com/HdrHistogram/HdrHistogram_c/archive/0.9.6.zip
            INSTALL_COMMAND ""
    )

    ExternalProject_Get_Property(hdr_histogram source_dir)
    set(HDRHISTOGRAM_SOURCE_DIR ${source_dir})
    ExternalProject_Get_Property(hdr_histogram binary_dir)
    set(HDRHISTOGRAM_BINARY_DIR ${binary_dir})

    set(HDRHISTOGRAM_LIBS
            ${HDRHISTOGRAM_BINARY_DIR}/src/${CMAKE_STATIC_LIBRARY_PREFIX}hdr_histogram_static${CMAKE_STATIC_LIBRARY_SUFFIX}
            )

    add_dependencies(ota-load-tests aktualizr hdr_histogram)

    target_include_directories(ota-load-tests PUBLIC
            ${HDRHISTOGRAM_SOURCE_DIR}/src)

    target_link_libraries(ota-load-tests
            aktualizr_static_lib
            ${HDRHISTOGRAM_LIBS}
            ${Boost_LIBRARIES}
            ${OPENSSL_LIBRARIES}
            ${sodium_LIBRARY_RELEASE}
            ${CMAKE_THREAD_LIBS_INIT}
            ${CURL_LIBRARIES}
            ${GLIB2_LIBRARIES}
            ${LibArchive_LIBRARIES}
            ${LIBOSTREE_LIBRARIES}
            ${SQLITE3_LIBRARIES}
            ${SYSTEMD_LIBRARY}
            ${LIBDPKG_LIBRARIES})

    install(TARGETS ota-load-tests
            COMPONENT aktualizr
            RUNTIME DESTINATION bin)
endif (BUILD_LOAD_TESTS)

aktualizr_source_file_checks(${LOAD_TESTS_SRC})