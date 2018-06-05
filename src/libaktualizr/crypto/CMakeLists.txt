set(SOURCES crypto.cc
            keymanager.cc)

set(HEADERS crypto.h
            keymanager_config.h
            keymanager.h
            openssl_compat.h)

set_source_files_properties(p11engine.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

add_library(crypto OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})

if(BUILD_P11)
    target_sources(crypto PRIVATE p11engine.cc)
    if(TEST_PKCS11_MODULE_PATH)
        add_definitions(-DTEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}" -DTEST_PKCS11_ENGINE_PATH="${TEST_PKCS11_ENGINE_PATH}")
    endif(TEST_PKCS11_MODULE_PATH)
else(BUILD_P11)
    target_sources(crypto PRIVATE p11engine_dummy.cc)
endif(BUILD_P11)


add_aktualizr_test(NAME crypto SOURCES crypto_test.cc PROJECT_WORKING_DIRECTORY)
add_aktualizr_test(NAME keymanager SOURCES keymanager_test.cc PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(p11engine.cc p11engine_dummy.cc p11_config.h p11engine.h ${TEST_SOURCES})
