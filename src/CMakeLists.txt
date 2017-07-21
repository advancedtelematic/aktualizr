
# set source files excluded main
set(SOURCES ../third_party/jsoncpp/jsoncpp.cpp
            aktualizr.cc
            logger.cc
            httpclient.cc
            config.cc
            events.cc
            commands.cc
            types.cc
            eventsinterpreter.cc
            gatewaymanager.cc
            crypto.cc
            utils.cc)
# set headers used for clang format
set(HEADERS
            aktualizr.h
            logger.h
            httpclient.h
            sotauptaneclient.h
            config.h
            channel.h
            events.h
            commands.h
            types.h
            eventsinterpreter.h
            gatewaymanager.h
            crypto.h
            openssl_compat.h
            ostree.h
            utils.h
            uptane/tuf.h
            uptane/tufrepository.h
            uptane/uptanerepository.h
            uptane/secondary.h
            uptane/testbusprimary.h
            uptane/testbussecondary.h
            uptane/exceptions.h)

if(BUILD_GENIVI)
    list(APPEND SOURCES dbusgateway/dbusgateway.cc
                        dbusgateway/swlm.cc
                        sotarviclient.cc)
    list(APPEND HEADERS dbusgateway/dbusgateway.h
                        dbusgateway/swlm.h)
endif(BUILD_GENIVI)

if (BUILD_OSTREE)
    list(APPEND SOURCES ostree.cc
                        sotauptaneclient.cc
                        uptane/role.cc
                        uptane/root.cc
                        uptane/tuf.cc
                        uptane/tufrepository.cc
                        uptane/uptanerepository.cc
                        uptane/secondary.cc
                        uptane/testbusprimary.cc
                        uptane/testbussecondary.cc)
endif(BUILD_OSTREE)


# set the name of the executable
add_executable(aktualizr main.cc)
add_library(aktualizr_static_lib STATIC ${SOURCES})

target_include_directories(aktualizr_static_lib PUBLIC ${LIBOSTREE_INCLUDE_DIRS})

target_link_libraries(aktualizr aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${SODIUM_LIBRARIES}
        ${LIBOSTREE_LIBRARIES}
        ${LIBDBUS_LIBRARIES}
        ${LibArchive_LIBRARIES} gcov)

if(BUILD_GENIVI)
    target_link_libraries(aktualizr rvi)
endif(BUILD_GENIVI)

################ QA RULES
if(CLANG_FORMAT)
    add_custom_target(format-src
        COMMAND ${CLANG_FORMAT} -i -style=file main.cc ${SOURCES} ${HEADERS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on src"
        VERBATIM)
    add_dependencies(qa format-src)
else()
    message(WARNING "clang-format not found, skipping")
endif()

if(CLANG_CHECK)
    add_custom_target(check-src
        COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} ${SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-check"
        VERBATIM)
    add_dependencies(qa check-src)
else()
    message(WARNING "clang-check not found, skipping")
endif(CLANG_CHECK)

# Export compile_commands.json for clang-check
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

################## INSTALL RULES
#configure_file(distribution/sota.service.cmake distribution/sota.service)
#install(FILES distribution/sota.service DESTINATION /etc/systemd/system  COMPONENT init)
install(TARGETS aktualizr RUNTIME DESTINATION bin)
if(BUILD_GENIVI)
    install(FILES dbus/org.genivi.SotaClient.conf DESTINATION /etc/dbus-1/system.d)
endif(BUILD_GENIVI)
if(INSTALL_LIB)
  install(TARGETS aktualizr_static_lib ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
  install(FILES ${HEADERS} DESTINATION include)
  install(DIRECTORY ../third_party/jsoncpp/json DESTINATION include)
  install(DIRECTORY ../third_party/picojson DESTINATION include)
  install(DIRECTORY uptane DESTINATION include FILES_MATCHING PATTERN "*.h")
endif()

# vim: set tabstop=4 shiftwidth=4 expandtab:
