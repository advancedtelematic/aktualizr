
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

aktualizr_source_file_checks(${GARAGE_PUSH_SRCS} ${ALL_SOTA_TOOLS_HEADERS})

target_compile_options(garage_push_static_lib PUBLIC -std=c++11)

target_include_directories(garage_push_static_lib INTERFACE ${PROJECT_SOURCE_DIR} ${GLIB2_INCLUDE_DIRS})

install(TARGETS garage-push DESTINATION /usr/bin)

# vim: set tabstop=2 shiftwidth=2 expandtab:
