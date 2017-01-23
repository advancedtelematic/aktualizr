# set symbols used when compiling
add_definitions(-DBOOST_LOG_DYN_LINK=1)

# find all required libraries
find_package(Boost COMPONENTS system thread program_options log log_setup regex REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_search_module(LIBCURL REQUIRED libcurl)
pkg_search_module(LIBOPENSSL REQUIRED libssl)

# configure libraries
set(LINK_LIBS ${CMAKE_THREAD_LIBS_INIT}
              ${Boost_SYSTEM_LIBRARY}
              ${Boost_THREAD_LIBRARY}
              ${Boost_LOG_LIBRARIES}
              ${Boost_LOG_SETUP_LIBRARIES}
              ${Boost_PROGRAM_OPTIONS_LIBRARY}
              ${Boost_REGEX_LIBRARY}
              ${LIBCURL_LIBRARIES}
              ${SYSTEMD_LIBRARIES})


# set source files excluded main for using the list for the test target
set(SOURCES third_party/jsoncpp/jsoncpp.cpp
            src/logger.cc
            src/httpclient.cc
            src/sotahttpclient.cc
            src/httpcommandinterpreter.cc
            src/config.cc
            src/events.cc
            src/commands.cc
            src/types.cc
            src/main.cc
            src/eventsinterpreter.cc)

set(HEADERS src/logger.h
            src/config.h
            src/interpreter.h
            src/httpclient.h
            src/sotahttpclient.h
            src/events.h
            src/commands.h
            src/types.h
            )


include_directories(third_party/jsoncpp)
set_property(SOURCE third_party/jsoncpp/jsoncpp.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -w")

if(BUILD_WITH_DBUS_GATEWAY)

set(DBUS_SOURCES    src/dbusgateway/dbusgateway.cc
                    src/dbusgateway/src-gen/v1/org/genivi/SotaClientStubDefault
                    src/dbusgateway/src-gen/v1/org/genivi/SotaClientDBusDeployment
                    src/dbusgateway/src-gen/v1/org/genivi/SotaClientDBusStubAdapter
                    src/dbusgateway/src-gen/v1/org/genivi/SoftwareLoadingManagerDBusProxy.cpp
                    src/dbusgateway/src-gen/v1/org/genivi/SoftwareLoadingManagerDBusDeployment
)

set(DBUS_HEADERS src/dbusgateway/dbusgateway.h )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++0x")
include_directories(
    src/dbusgateway/src-gen
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build/include/)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/commonapi_install/include/CommonAPI-3.1)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dbusapi_install/include/CommonAPI-3.1)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dbusapi_install/include)
link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/commonapi_install/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/dbusapi_install/lib
)

set(DBUS_LIBS CommonAPI CommonAPI-DBus dbus-1)
endif(BUILD_WITH_DBUS_GATEWAY)

# set output folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY target/)

# set the name of the executable
add_executable(sota_client ${SOURCES} ${DBUS_SOURCES})

# define libraries for the target
target_link_libraries(sota_client ${LINK_LIBS} ${DBUS_LIBS})

if(BUILD_WITH_DBUS_GATEWAY)
    add_dependencies(sota_client dbusapi)
    add_dependencies(dbusapi commonapi libdbus)
endif(BUILD_WITH_DBUS_GATEWAY)

configure_file(distribution/sota.service.cmake distribution/sota.service)
install(FILES distribution/sota.service DESTINATION /etc/systemd/system  COMPONENT init)
install(TARGETS sota_client RUNTIME  DESTINATION bin)
install(FILES  config/config.toml.example DESTINATION /etc/  RENAME sota.conf  COMPONENT configuration)

if(BUILD_WITH_DBUS_GATEWAY)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libdbus_build/lib/ DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/commonapi_install/lib/ DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dbusapi_install/lib/ DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif(BUILD_WITH_DBUS_GATEWAY)
