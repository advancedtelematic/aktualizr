# set the name of the executable
add_executable(aktualizr_cert_provider main.cc)

target_link_libraries(aktualizr_cert_provider aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${sodium_LIBRARY_RELEASE}
        ${LIBP11_LIBRARIES}
        ${LibArchive_LIBRARIES} gcov)

add_custom_target(check-format-cp
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/check-formatting.sh ${CLANG_FORMAT} main.cc
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking clang-format on src/cert_provider"
    VERBATIM)
add_dependencies(check-format check-format-cp)

if(CLANG_FORMAT)
    add_custom_target(format-cp
        COMMAND ${CLANG_FORMAT} -i -style=file main.cc
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on src/cert_provider"
        VERBATIM)
    add_dependencies(format format-cp)
else()
    message(WARNING "clang-format not found, skipping")
endif()

if(CLANG_CHECK)
    add_custom_target(check-cp
        COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} main.cc
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-check on src/cert_provider"
        VERBATIM)
    add_dependencies(qa check-cp)
else()
    message(WARNING "clang-check not found, skipping")
endif(CLANG_CHECK)

install(TARGETS aktualizr_cert_provider RUNTIME DESTINATION bin)

# vim: set tabstop=4 shiftwidth=4 expandtab:
