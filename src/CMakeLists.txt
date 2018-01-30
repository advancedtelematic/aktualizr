set_source_files_properties(${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS -w)
set_source_files_properties(${PROJECT_SOURCE_DIR}/src/p11engine.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)


# set source files excluding main
set(SOURCES aktualizr.cc
            bootstrap.cc
            commands.cc
            config.cc
            crypto.cc
            events.cc
            eventsinterpreter.cc
            invstorage.cc
            fsstorage.cc
            sqlstorage.cc
            gatewaymanager.cc
            httpclient.cc
            logging.cc
            socketgateway.cc
            timer.cc
            types.cc
            utils.cc)

# set headers used for clang format
set(HEADERS
            aktualizr.h
            bootstrap.h
            channel.h
            commands.h
            config.h
            crypto.h
            events.h
            eventsinterpreter.h
            fsstorage.h
            sqlstorage.h
            gatewaymanager.h
            httpclient.h
            httpinterface.h
            invstorage.h
            logging.h
            openssl_compat.h
            ostree.h
            ostreefake.h
            ostreeinterface.h
            packagemanagerfactory.h
            packagemanagerinterface.h
            socketgateway.h
            sotauptaneclient.h
            timer.h
            types.h
            utils.h
            exceptions.h
            uptane/exceptions.h
            uptane/legacysecondary.h
            uptane/managedsecondary.h
            uptane/secondaryconfig.h
            uptane/secondaryfactory.h
            uptane/secondaryinterface.h
            uptane/tuf.h
            uptane/tufrepository.h
            uptane/uptanerepository.h
            uptane/virtualsecondary.h)

if(BUILD_GENIVI)
    list(APPEND SOURCES dbusgateway/dbusgateway.cc
                        dbusgateway/swlm.cc
                        sotarviclient.cc)
    list(APPEND HEADERS dbusgateway/dbusgateway.h
                        dbusgateway/swlm.h)
endif(BUILD_GENIVI)

if(BUILD_OSTREE)
    list(APPEND SOURCES ostree.cc
                        sotauptaneclient.cc
                        uptane/initialize.cc
                        uptane/legacysecondary.cc
                        uptane/managedsecondary.cc
                        uptane/role.cc
                        uptane/root.cc
                        uptane/tuf.cc
                        uptane/tufrepository.cc
                        uptane/uptanerepository.cc
                        uptane/virtualsecondary.cc
                        uptane/cryptokey.cc)
endif(BUILD_OSTREE)

if(BUILD_P11)
    list(APPEND SOURCES p11engine.cc)
    list(APPEND HEADERS p11engine.h)

    if(TEST_PKCS11_MODULE_PATH)
        add_definitions(-DTEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}" -DTEST_PKCS11_ENGINE_PATH="${TEST_PKCS11_ENGINE_PATH}")
    endif(TEST_PKCS11_MODULE_PATH)
endif(BUILD_P11)
# set the name of the executable
add_executable(aktualizr main.cc)
add_library(aktualizr_static_lib STATIC ${SOURCES} ${PROJECT_SOURCE_DIR}/third_party/jsoncpp/jsoncpp.cpp)
target_include_directories(aktualizr_static_lib PUBLIC ${LIBOSTREE_INCLUDE_DIRS})

target_link_libraries(aktualizr aktualizr_static_lib
        ${Boost_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${sodium_LIBRARY_RELEASE}
        ${LIBOSTREE_LIBRARIES}
        ${LIBDBUS_LIBRARIES}
        ${LIBP11_LIBRARIES}
        ${SQLITE3_LIBRARIES}
        ${LibArchive_LIBRARIES})

if(BUILD_WITH_CODE_COVERAGE)
    target_link_libraries(aktualizr gcov)
endif(BUILD_WITH_CODE_COVERAGE)

if(BUILD_GENIVI)
    target_link_libraries(aktualizr rvi)
endif(BUILD_GENIVI)

################ QA RULES
aktualizr_source_file_checks(main.cc ${SOURCES} ${HEADERS})

################## INSTALL RULES
install(FILES ${PROJECT_SOURCE_DIR}/config/systemd/aktualizr-ubuntu.service DESTINATION /lib/systemd/system RENAME aktualizr.service  COMPONENT aktualizr)
install(FILES ${PROJECT_SOURCE_DIR}/config/sota_ubuntu.toml DESTINATION /usr/lib/sota RENAME sota.toml  COMPONENT aktualizr)
install(TARGETS aktualizr RUNTIME DESTINATION bin COMPONENT aktualizr)
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
