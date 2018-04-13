set(SOURCES aktualizr_secondary_ipc.cc
            asn1-cer.cc
            asn1-cerstream.cc)

set(HEADERS aktualizr_secondary_ipc.h
            asn1-cer.h
            asn1-cerstream.h)


add_library(aktualizr_secondary_ipc OBJECT ${SOURCES})

target_include_directories(aktualizr_secondary_ipc PUBLIC
    $<TARGET_PROPERTY:socket_activation,INCLUDE_DIRECTORIES>)

aktualizr_source_file_checks(${SOURCES} ${HEADERS})