add_subdirectory("asn1")

set(SOURCES ipuptanesecondary.cc)

set(HEADERS ipuptanesecondary.h)

add_library(aktualizr-posix STATIC ${SOURCES})

get_property(ASN1_INCLUDE_DIRS TARGET asn1_lib PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(aktualizr-posix PUBLIC ${ASN1_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})

aktualizr_source_file_checks(${HEADERS} ${SOURCES})
