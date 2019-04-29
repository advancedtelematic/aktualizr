add_subdirectory("asn1")

set(SOURCES ipuptanesecondary.cc)

set(HEADERS ipuptanesecondary.h)

set(TARGET aktualizr-posix)

add_library(${TARGET} STATIC
  ${SOURCES}
  $<TARGET_OBJECTS:asn1>
  $<TARGET_OBJECTS:asn1_lib>
)

get_property(ASN1_INCLUDE_DIRS TARGET asn1_lib PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(${TARGET} PUBLIC ${ASN1_INCLUDE_DIRS})

target_link_libraries(${TARGET} aktualizr_static_lib)


aktualizr_source_file_checks(${HEADERS} ${SOURCES})
