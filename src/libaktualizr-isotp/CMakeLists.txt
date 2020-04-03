set(SOURCES isotpsecondary.cc isotp_conn.cc isotp_allocate.cc)
set(HEADERS isotpsecondary.h isotp_conn.h)
if(BUILD_ISOTP)
    set(ISOTP_PATH_PREFIX ${PROJECT_SOURCE_DIR}/third_party/isotp-c/src)
    set(BITFIELD_PATH_PREFIX ${ISOTP_PATH_PREFIX}/../deps/bitfield-c/src)
    set(UPTANE_PATH_PREFIX ${PROJECT_SOURCE_DIR}/src/libaktualizr/uptane)

    set(ISOTP_SOURCES ${ISOTP_PATH_PREFIX}/isotp/isotp.c
        ${ISOTP_PATH_PREFIX}/isotp/send.c
        ${ISOTP_PATH_PREFIX}/isotp/receive.c
        ${BITFIELD_PATH_PREFIX}/bitfield/8byte.c
        ${BITFIELD_PATH_PREFIX}/bitfield/bitarray.c
        ${BITFIELD_PATH_PREFIX}/bitfield/bitfield.c)

    set_source_files_properties(${ISOTP_SOURCES} PROPERTIES COMPILE_FLAGS "-Wno-sign-conversion -Wno-conversion -Wno-error=unused-parameter")

    add_library(aktualizr-socketcan STATIC ${SOURCES})

    target_include_directories(aktualizr-socketcan PUBLIC ${ISOTP_PATH_PREFIX} ${UPTANE_PATH_PREFIX} ${BITFIELD_PATH_PREFIX} ${CMAKE_CURRENT_SOURCE_DIR})

endif(BUILD_ISOTP)
aktualizr_source_file_checks(${HEADERS} ${SOURCES})
