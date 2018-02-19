# set the name of the executable
add_executable(aktualizr_cert_provider main.cc)

set_source_files_properties(main.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

target_link_libraries(aktualizr_cert_provider aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${sodium_LIBRARY_RELEASE}
        ${LIBP11_LIBRARIES}
        ${LibArchive_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr_cert_provider gcov)
endif(BUILD_WITH_CODE_COVERAGE)

aktualizr_source_file_checks(main.cc)

add_dependencies(build_tests aktualizr_cert_provider)

install(TARGETS aktualizr_cert_provider RUNTIME DESTINATION bin)

# vim: set tabstop=4 shiftwidth=4 expandtab:
