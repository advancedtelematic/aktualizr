
set (SOTA_TOOLS_LIB_SRC
  authenticate.cc
  logging.cc
  oauth2.cc
  ostree_hash.cc
  ostree_ref.cc
  ostree_dir_repo.cc
  ostree_http_repo.cc
  ostree_object.cc
  request_pool.cc
  treehub_server.cc
)

add_library(sota_tools_static_lib STATIC ${SOTA_TOOLS_LIB_SRC})


##### garage-push targets

set (GARAGE_PUSH_SRCS
  garage_push.cc
)

add_executable(garage-push ${GARAGE_PUSH_SRCS})

# Export compile_commands.json for clang-check
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_link_libraries(garage-push sota_tools_static_lib aktualizr_static_lib
  ${CURL_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${LibArchive_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(garage-push gcov)
endif(BUILD_WITH_CODE_COVERAGE)


target_compile_options(sota_tools_static_lib PUBLIC -std=c++11)

target_include_directories(sota_tools_static_lib PUBLIC ${PROJECT_SOURCE_DIR} ${GLIB2_INCLUDE_DIRS})

install(TARGETS garage-push DESTINATION /usr/bin)


##### garage-deploy targets

set (GARAGE_DEPLOY_SRCS
  garage_deploy.cc
)

set(Boost_USE_STATIC_LIBS   ON)
find_package(Boost COMPONENTS program_options)

add_executable(garage-deploy ${GARAGE_DEPLOY_SRCS})
target_link_libraries(garage-deploy ${Boost_LIBRARIES})

if(GARAGE_DEPLOY_VERSION)
    add_definitions(-DGARAGE_DEPLOY_VERSION="${GARAGE_DEPLOY_VERSION}")
else(GARAGE_DEPLOY_VERSION)
    add_definitions(-DGARAGE_DEPLOY_VERSION="dev")
endif(GARAGE_DEPLOY_VERSION)

aktualizr_source_file_checks(${GARAGE_DEPLOY_SRCS})

add_dependencies(build_tests garage-deploy)

install(TARGETS garage-deploy DESTINATION bin COMPONENT garage_deploy)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Serhiy Stetskovych<serhiy.stetskovych@innoteka.com>") #required
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "garage-deploy utility")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL garage_deploy)

if(GARAGE_DEPLOY_VERSION)
    set(CPACK_PACKAGE_VERSION "${GARAGE_DEPLOY_VERSION}")
else(GARAGE_DEPLOY_VERSION)
    set(CPACK_PACKAGE_VERSION "1.0")
endif(GARAGE_DEPLOY_VERSION)
include(CPack)
add_dependencies(qa package)


##### clang-format

aktualizr_source_file_checks(${GARAGE_PUSH_SRCS} ${ALL_SOTA_TOOLS_HEADERS} ${SOTA_TOOLS_LIB_SRC})
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

# vim: set tabstop=2 shiftwidth=2 expandtab:
