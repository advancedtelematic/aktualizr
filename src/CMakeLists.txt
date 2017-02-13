# set symbols used when compiling
add_definitions(-DBOOST_LOG_DYN_LINK=1)

# find all required libraries
find_package(Boost COMPONENTS system thread program_options log log_setup regex REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_search_module(LIBCURL REQUIRED libcurl)
pkg_search_module(LIBDBUS REQUIRED dbus-1)
pkg_search_module(LIBOPENSSL REQUIRED libssl)

# configure libraries
set(LINK_LIBS ${CMAKE_THREAD_LIBS_INIT}
              ${Boost_SYSTEM_LIBRARY}
              ${Boost_THREAD_LIBRARY}
              ${Boost_LOG_LIBRARIES}
              ${Boost_LOG_SETUP_LIBRARIES}
              ${Boost_PROGRAM_OPTIONS_LIBRARY}
              ${Boost_REGEX_LIBRARY}
              ${LIBCURL_LIBRARIES})


# set source files excluded main for using the list for the test target
set(SOURCES third_party/jsoncpp/jsoncpp.cpp
            src/logger.cc
            src/httpclient.cc
            src/sotahttpclient.cc
            src/commandinterpreter.cc
            src/config.cc
            src/events.cc
            src/commands.cc
            src/types.cc
            src/main.cc
            src/sotarviclient.cc
            src/eventsinterpreter.cc
            src/gatewaymanager.cc
            src/socketgateway.cc)

            
if(BUILD_WITH_DBUS_GATEWAY)
    set(DBUS_SOURCES    src/dbusgateway/dbusgateway.cc
                        src/dbusgateway/swlm.cc)
endif(BUILD_WITH_DBUS_GATEWAY)


include_directories(third_party/jsoncpp)
include_directories(third_party/picojson)
set_property(SOURCE third_party/jsoncpp/jsoncpp.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -w")


ExternalProject_Get_Property(rvi_lib source_dir)
set(RVI_INCLUDE_DIR ${source_dir}/include)
include_directories(${RVI_INCLUDE_DIR})
set(RVI_LINK_DIR ${source_dir}/src/.libs)
link_directories(${RVI_LINK_DIR})

# set output folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY target/)

# set the name of the executable
add_executable(aktualizr ${SOURCES} ${DBUS_SOURCES})

if(BUILD_WITH_DBUS_GATEWAY)

    if(STAGING_DIR_TARGET)
        set(DBUS_INCLUDE_DIRS ${STAGING_DIR_TARGET}/usr/include/dbus-1.0/ ${STAGING_DIR_TARGET}/usr/lib/dbus-1.0/include)
        include_directories(${DBUS_INCLUDE_DIRS})
    else()
        target_compile_options(sota_client PUBLIC ${LIBDBUS_CFLAGS})
    endif()
    
    set(DBUS_LIBS ${LIBDBUS_LIBRARIES})
endif(BUILD_WITH_DBUS_GATEWAY)

target_link_libraries(aktualizr ${LINK_LIBS} ${DBUS_LIBS} rvi)
add_dependencies(aktualizr rvi_lib)


################## INSTALL RULES
configure_file(distribution/sota.service.cmake distribution/sota.service)
install(FILES distribution/sota.service DESTINATION /etc/systemd/system  COMPONENT init)
install(TARGETS aktualizr RUNTIME  DESTINATION bin)
install(FILES  config/config.toml.example DESTINATION /etc/  RENAME sota.conf  COMPONENT configuration)
