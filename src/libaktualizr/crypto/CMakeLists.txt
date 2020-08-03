set(SOURCES crypto.cc
            keymanager.cc)

set(HEADERS crypto.h
            keymanager.h
            openssl_compat.h)

set_source_files_properties(p11engine.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

add_library(crypto OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})

if(BUILD_P11)
    target_sources(crypto PRIVATE p11engine.cc)
    if(PKCS11_ENGINE_PATH)
        set_property(SOURCE p11engine.cc PROPERTY COMPILE_DEFINITIONS PKCS11_ENGINE_PATH="${PKCS11_ENGINE_PATH}")
    endif(PKCS11_ENGINE_PATH)
else(BUILD_P11)
    target_sources(crypto PRIVATE p11engine_dummy.cc)
endif(BUILD_P11)

add_aktualizr_test(NAME crypto SOURCES crypto_test.cc PROJECT_WORKING_DIRECTORY)
add_aktualizr_test(NAME hash SOURCES hash_test.cc PROJECT_WORKING_DIRECTORY)
add_aktualizr_test(NAME keymanager SOURCES keymanager_test.cc PROJECT_WORKING_DIRECTORY)
set_property(SOURCE crypto_test.cc keymanager_test.cc PROPERTY COMPILE_DEFINITIONS TEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}")

set_tests_properties(test_crypto test_hash test_keymanager PROPERTIES LABELS "crypto")

aktualizr_source_file_checks(p11engine.cc p11engine_dummy.cc p11engine.h ${TEST_SOURCES})
