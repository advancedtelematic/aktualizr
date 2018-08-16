set(ISOTP_PATH_PREFIX ${PROJECT_SOURCE_DIR}/partial/extern/isotp-c/src)
set(BITFIELD_PATH_PREFIX ${ISOTP_PATH_PREFIX}/../deps/bitfield-c/src)

set(ISOTP_SOURCES ${ISOTP_PATH_PREFIX}/isotp/isotp.c
    ${ISOTP_PATH_PREFIX}/isotp/send.c
    ${ISOTP_PATH_PREFIX}/isotp/receive.c
    ${BITFIELD_PATH_PREFIX}/bitfield/8byte.c
    ${BITFIELD_PATH_PREFIX}/bitfield/bitarray.c
    ${BITFIELD_PATH_PREFIX}/bitfield/bitfield.c)

set_source_files_properties(${ISOTP_SOURCES} PROPERTIES COMPILE_FLAGS "-Wno-sign-conversion -Wno-conversion -Wno-error=unused-parameter")

set(SOURCES isotp_conn.cc isotp_allocate.cc)

set(HEADERS isotp_conn.h)

add_library(isotp_conn OBJECT ${SOURCES} ${ISOTP_SOURCES})
target_include_directories(isotp_conn PUBLIC ${ISOTP_PATH_PREFIX} ${BITFIELD_PATH_PREFIX})

aktualizr_source_file_checks(${SOURCES} ${HEADERS})
