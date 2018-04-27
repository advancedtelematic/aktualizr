set(SOURCES httpclient.cc)

set(HEADERS httpclient.h
            httpinterface.h)

add_library(http OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
