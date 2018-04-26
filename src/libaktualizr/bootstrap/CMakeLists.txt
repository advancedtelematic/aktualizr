set(HEADERS bootstrap.h)
set(SOURCES bootstrap.cc)

add_library(bootstrap OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
