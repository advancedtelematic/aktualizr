set(SOURCES crypto.cc
            httpclient.cc
            keymanager.cc
            logging.cc
            types.cc
            utils.cc)

set(HEADERS crypto.h
            httpclient.h
            keymanager.h
            logging.h
            types.h
            utils.h)


add_library(utilities OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})

if(BUILD_P11)
    target_sources(package_manager PUBLIC p11engine.cc)
    if(TEST_PKCS11_MODULE_PATH)
        add_definitions(-DTEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}" -DTEST_PKCS11_ENGINE_PATH="${TEST_PKCS11_ENGINE_PATH}")
    endif(TEST_PKCS11_MODULE_PATH)
endif(BUILD_P11)

aktualizr_source_file_checks(p11engine.cc p11engine.h)