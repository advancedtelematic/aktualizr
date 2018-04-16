set(SOURCES asn1-cer.cc
            asn1-cerstream.cc)

set(HEADERS asn1-cer.h
            asn1-cerstream.h)

add_library(asn1 OBJECT ${SOURCES})

aktualizr_source_file_checks(${SOURCES} ${HEADERS})