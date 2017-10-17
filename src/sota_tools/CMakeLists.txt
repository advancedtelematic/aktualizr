
set (GARAGE_PUSH_SRCS
  authenticate.cc
  logging.cc
  main.cc
  oauth2.cc
  ostree_hash.cc
  ostree_ref.cc
  ostree_dir_repo.cc
  ostree_http_repo.cc
  ostree_object.cc
  request_pool.cc
  treehub_server.cc
)


add_executable(garage-push main.cc)
add_library(garage_push_static_lib STATIC ${GARAGE_PUSH_SRCS})
target_link_libraries(garage-push garage_push_static_lib aktualizr_static_lib
  ${CURL_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${LibArchive_LIBRARIES}
  gcov)

set (ALL_SOTA_TOOLS_SRCS
  ${GARAGE_PUSH_SRCS})

# For clang-format
set(ALL_SOTA_TOOLS_HEADERS
  accumulator.h
  authenticate.h
  logging.h
  oauth2.h
  ostree_hash.h
  ostree_object.h
  ostree_ref.h
  ostree_repo.h
  ostree_dir_repo.h
  request_pool.h
  treehub_server.h
)

# Export compile_commands.json for clang-check
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_target(check-format-sota-tools
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/check-formatting.sh ${CLANG_FORMAT} ${ALL_SOTA_TOOLS_SRCS} ${ALL_SOTA_TOOLS_HEADERS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking clang-format on src/sota-tools"
    VERBATIM)
add_dependencies(check-format check-format-sota-tools)

if(CLANG_FORMAT)
add_custom_target(format-sota-tools
    COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_SOTA_TOOLS_SRCS} ${ALL_SOTA_TOOLS_HEADERS}
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-formaton src/sota_tools"
    VERBATIM)
add_dependencies(format format-sota-tools)
else()
    message(WARNING "clang-format not found, skipping")
endif()

if(CLANG_CHECK)
    add_custom_target(check-tools
        COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} ${ALL_SRCS}
        WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-check"
        VERBATIM)
    add_dependencies(qa check-tools)
else()
    message(WARNING "clang-check not found, skipping")
endif(CLANG_CHECK)

target_compile_options(garage_push_static_lib PUBLIC -std=c++11)


target_include_directories(garage_push_static_lib INTERFACE ${PROJECT_SOURCE_DIR} ${GLIB2_INCLUDE_DIRS})


install(TARGETS garage-push DESTINATION /usr/bin)

# vim: set tabstop=2 shiftwidth=2 expandtab:
