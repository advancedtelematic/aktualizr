add_subdirectory("asn1")

set(SOURCES ipuptanesecondary.cc)

set(HEADERS ipuptanesecondary.h)

add_library(aktualizr-posix OBJECT ${SOURCES})

get_property(ASN1_INCLUDE_DIRS TARGET asn1_lib PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(aktualizr-posix PUBLIC ${ASN1_INCLUDE_DIRS})

aktualizr_source_file_checks(${HEADERS} ${SOURCES})
