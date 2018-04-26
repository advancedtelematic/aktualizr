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
    if(TEST_PKCS11_MODULE_PATH)
        add_definitions(-DTEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}" -DTEST_PKCS11_ENGINE_PATH="${TEST_PKCS11_ENGINE_PATH}")
    endif(TEST_PKCS11_MODULE_PATH)
endif(BUILD_P11)

aktualizr_source_file_checks(p11engine.cc p11engine.h)
