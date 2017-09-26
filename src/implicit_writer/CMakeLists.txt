set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/jsoniwp/jsoniwp.iwp PROPERTIES COMPILE_FLAGS -w)

# set the name of the executable
add_executable(aktualizr_implicit_writer main.cc)

target_link_libraries(aktualizr_implicit_writer aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${sodium_LIBRARY_RELEASE}
        ${LibArchive_LIBRARIES} gcov)

add_custom_target(check-format-iw
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/check-formatting.sh ${CLANG_FORMAT} main.cc
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking clang-format on src/implicit_writer"
    VERBATIM)
add_dependencies(check-format check-format-iw)

if(CLANG_FORMAT)
    add_custom_target(format-iw
        COMMAND ${CLANG_FORMAT} -i -style=file main.cc
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on src/implicit_writer"
        VERBATIM)
    add_dependencies(format format-iw)
else()
    message(WARNING "clang-format not found, skipping")
endif()

if(CLANG_CHECK)
    add_custom_target(check-iw
        COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} main.cc
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-check on src/implicit_writer"
        VERBATIM)
    add_dependencies(qa check-iw)
else()
    message(WARNING "clang-check not found, skipping")
endif(CLANG_CHECK)

