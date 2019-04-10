set(SOURCES asn1-cer.cc
            asn1-cerstream.cc
            asn1_message.cc)

set(HEADERS asn1-cer.h
            asn1-cerstream.h
            asn1_message.h)

add_library(asn1 OBJECT ${SOURCES})


compile_asn1_lib(SOURCES
    messages/common.asn1
    messages/cryptosource.asn1
    messages/ipuptane_message.asn1
    messages/tlsconfig.asn1
)

add_aktualizr_test(NAME asn1 LIBRARIES asn1 asn1_lib SOURCES asn1_test.cc)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})
