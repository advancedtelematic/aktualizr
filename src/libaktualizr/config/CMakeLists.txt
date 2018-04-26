set(HEADERS config.h)
set(SOURCES config.cc)

add_library(config OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
