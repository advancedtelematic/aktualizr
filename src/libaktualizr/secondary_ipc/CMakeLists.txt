set(SOURCES aktualizr_secondary_ipc.cc)

set(HEADERS aktualizr_secondary_ipc.h)


add_library(aktualizr_secondary_ipc OBJECT ${SOURCES})

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})