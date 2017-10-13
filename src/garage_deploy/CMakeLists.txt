set (GARAGE_DEPLOY_SRCS
  main.cc
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

# vim: set tabstop=4 shiftwidth=4 expandtab: