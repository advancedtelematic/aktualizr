
set (SOTA_TOOLS_LIB_SRC
  authenticate.cc
  server_credentials.cc
  logging.cc
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
set_property(TARGET sota_tools_static_lib PROPERTY CXX_STANDARD 11)

##### garage-push targets

set (GARAGE_PUSH_SRCS
  garage_push.cc
)

add_executable(garage-push ${GARAGE_PUSH_SRCS})
set_property(TARGET garage-push PROPERTY CXX_STANDARD 11)

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
set_property(TARGET garage-check PROPERTY CXX_STANDARD 11)

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
set_property(TARGET garage-deploy PROPERTY CXX_STANDARD 11)
target_link_libraries(garage-deploy sota_tools_static_lib aktualizr_static_lib
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CURL_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${LibArchive_LIBRARIES}
  ${LIBOSTREE_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(garage-deploy gcov)
endif(BUILD_WITH_CODE_COVERAGE)

if(GARAGE_DEPLOY_VERSION)
    add_definitions(-DGARAGE_DEPLOY_VERSION="${GARAGE_DEPLOY_VERSION}")
else(GARAGE_DEPLOY_VERSION)
    add_definitions(-DGARAGE_DEPLOY_VERSION="dev")
endif(GARAGE_DEPLOY_VERSION)


aktualizr_source_file_checks(${GARAGE_DEPLOY_SRCS})

add_dependencies(build_tests garage-deploy)


install(TARGETS garage-deploy RUNTIME DESTINATION bin COMPONENT garage_deploy)

include(ExternalProject)
ExternalProject_Add(garage-sign DEPENDS garage-deploy URL https://ats-tuf-cli-releases.s3-eu-central-1.amazonaws.com/cli-0.2.0-59-gbb29ed5.tgz
URL_HASH SHA256=3175e4ca7eb3fb72d99e468a6f6827499b3d9da3fccdc1dcf36c2cbd90aa8f0e
CONFIGURE_COMMAND echo "Skipping configure"
BUILD_COMMAND echo "Skipping build"
INSTALL_COMMAND echo "Skipping install")

ExternalProject_Get_Property(garage-sign SOURCE_DIR)

install(PROGRAMS ${SOURCE_DIR}/bin/garage-sign DESTINATION bin COMPONENT garage_deploy)
install(DIRECTORY ${SOURCE_DIR}/lib DESTINATION . COMPONENT garage_deploy)

add_custom_target(garage-deploy-deb COMMAND cpack WORKING_DIRECTORY ${CMAKE_BINARY_DIR} DEPENDS garage-deploy garage-sign)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Serhiy Stetskovych<serhiy.stetskovych@innoteka.com>") #required
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "garage-deploy utility")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL garage_deploy garage-sign)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_PACKAGE_FILE_NAME garage_deploy)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libostree-1-1, libsodium18, openjdk-8-jre")


if(GARAGE_DEPLOY_VERSION)
    set(CPACK_PACKAGE_VERSION "${GARAGE_DEPLOY_VERSION}")
else(GARAGE_DEPLOY_VERSION)
    set(CPACK_PACKAGE_VERSION "1.0")
endif(GARAGE_DEPLOY_VERSION)
include(CPack)
add_dependencies(qa garage-deploy-deb)


##### clang-format

# For clang-format
set(ALL_SOTA_TOOLS_HEADERS
  accumulator.h
  authenticate.h
  deploy.h
  logging.h
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
