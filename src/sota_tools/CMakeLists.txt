
set (SOTA_TOOLS_LIB_SRC
  authenticate.cc
  server_credentials.cc
  oauth2.cc
  ostree_hash.cc
  ostree_ref.cc
  ostree_dir_repo.cc
  ostree_http_repo.cc
  ostree_object.cc
  request_pool.cc
  treehub_server.cc
  deploy.cc
)

add_library(sota_tools_static_lib STATIC ${SOTA_TOOLS_LIB_SRC})

##### garage-push targets

set (GARAGE_PUSH_SRCS
  garage_push.cc
)

add_executable(garage-push ${GARAGE_PUSH_SRCS})

# Export compile_commands.json for clang-check
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
target_compile_options(sota_tools_static_lib PUBLIC -std=c++11)

target_include_directories(sota_tools_static_lib PUBLIC ${PROJECT_SOURCE_DIR} ${GLIB2_INCLUDE_DIRS})

target_link_libraries(garage-push sota_tools_static_lib aktualizr_static_lib
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CURL_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${LibArchive_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(garage-push gcov)
endif(BUILD_WITH_CODE_COVERAGE)

install(TARGETS garage-push RUNTIME DESTINATION bin)

##### garage-check targets
set (GARAGE_CHECK_SRCS
  garage_check.cc
)

add_executable(garage-check ${GARAGE_CHECK_SRCS})

target_link_libraries(garage-check sota_tools_static_lib aktualizr_static_lib
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CURL_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${LibArchive_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(garage-check gcov)
endif(BUILD_WITH_CODE_COVERAGE)

install(TARGETS garage-check RUNTIME DESTINATION bin)

##### garage-deploy targets

set (GARAGE_DEPLOY_SRCS
  garage_deploy.cc
)

add_executable(garage-deploy ${GARAGE_DEPLOY_SRCS})
target_link_libraries(garage-deploy sota_tools_static_lib aktualizr_static_lib
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CURL_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${LibArchive_LIBRARIES}
  ${LIBOSTREE_LIBRARIES}
  ${LIBDPKG_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(garage-deploy gcov)
endif(BUILD_WITH_CODE_COVERAGE)

add_definitions(-DGARAGE_DEPLOY_VERSION="${AKTUALIZR_VERSION}")


aktualizr_source_file_checks(${GARAGE_DEPLOY_SRCS})

add_dependencies(build_tests garage-deploy)


install(TARGETS garage-deploy RUNTIME DESTINATION bin COMPONENT garage_deploy)

include(ExternalProject)
ExternalProject_Add(garage-sign DEPENDS garage-deploy
                    URL https://ats-tuf-cli-releases.s3-eu-central-1.amazonaws.com/cli-0.2.0-106-g613f9ad.tgz
                    URL_HASH SHA256=afd869c21e2a37918eef95e0ec778b54e86a2e725a0dc0ae982be46b9c790ff6
                    CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "")
ExternalProject_Get_Property(garage-sign SOURCE_DIR)

install(PROGRAMS ${SOURCE_DIR}/bin/garage-sign DESTINATION bin COMPONENT garage_deploy)
install(DIRECTORY ${SOURCE_DIR}/lib DESTINATION . COMPONENT garage_deploy)


##### clang-format

# For clang-format
set(ALL_SOTA_TOOLS_HEADERS
  accumulator.h
  authenticate.h
  deploy.h
  oauth2.h
  ostree_dir_repo.h
  ostree_hash.h
  ostree_http_repo.h
  ostree_object.h
  ostree_ref.h
  ostree_repo.h
  request_pool.h
  server_credentials.h
  treehub_server.h
)

aktualizr_source_file_checks(${GARAGE_PUSH_SRCS} ${GARAGE_CHECK_SRCS} ${ALL_SOTA_TOOLS_HEADERS} ${SOTA_TOOLS_LIB_SRC})


# vim: set tabstop=2 shiftwidth=2 expandtab:
