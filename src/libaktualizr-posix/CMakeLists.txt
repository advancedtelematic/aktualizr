add_subdirectory("asn1")

set(SOURCES
  ipuptanesecondary.cc
  ipsecondarydiscovery.cc)

set(HEADERS
  ipuptanesecondary.h
  ipsecondarydiscovery.h)

set(TARGET aktualizr-posix)

add_library(${TARGET} STATIC
  ${SOURCES}
  $<TARGET_OBJECTS:asn1>
  $<TARGET_OBJECTS:asn1_lib>
)

get_property(ASN1_INCLUDE_DIRS TARGET asn1_lib PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(${TARGET} PUBLIC ${ASN1_INCLUDE_DIRS})

target_link_libraries(${TARGET} aktualizr_static_lib)

add_aktualizr_test(NAME discovery_secondary
                   SOURCES ipsecondary_discovery_test.cc
                   LIBRARIES ${TARGET}
                   PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${HEADERS} ${SOURCES} ipsecondary_discovery_test.cc)
