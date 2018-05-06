set(SOURCES asn1-cer.cc
            asn1-cerstream.cc)

set(HEADERS asn1-cer.h
            asn1-cerstream.h)

add_library(asn1 OBJECT ${SOURCES})

include(AddAktualizrTest)

# Reference asn1c libraries
add_asn1c_lib(${PROJECT_SOURCE_DIR}/tests/asn1-reference/common ${PROJECT_SOURCE_DIR}/tests/asn1-reference/config/cryptosource ${PROJECT_SOURCE_DIR}/tests/asn1-reference/config/tlsconfig)
add_asn1c_lib(${PROJECT_SOURCE_DIR}/tests/asn1-reference/uptane/ipuptane_message)
compile_asn1_lib()
list(APPEND TEST_LIBS asn1_lib)

add_aktualizr_test(NAME asn1 SOURCES asn1_test.cc ARGS ${PROJECT_SOURCE_DIR}/build5/tests PROJECT_WORKING_DIRECTORY)
aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES} )