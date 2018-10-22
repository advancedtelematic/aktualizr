find_program(ASN1C NAMES asn1c)

if(ASN1C MATCHES ".*-NOTFOUND")
    message(ERROR "asn1c not found")
endif(ASN1C MATCHES ".*-NOTFOUND")
message(STATUS "Found asn1c: ${ASN1C}")

# -fnative-types is required for compat with asn1c <= 0.9.24
# -fline-refs isn't available on Ubuntu 16.04
set(ASN1C_FLAGS ${ASN1C_FLAGS} -fskeletons-copy -fnative-types -pdu=all)

if (ASN1C_SKELETONS_DIR)
  set(ASN1C_FLAGS ${ASN1C_FLAGS} -S ${ASN1C_SKELETONS_DIR})
endif (ASN1C_SKELETONS_DIR)

define_property(GLOBAL PROPERTY ASN1_FILES_GLOBAL
                BRIEF_DOCS "List of all input files for asn1c"
                FULL_DOCS "List of all input files for asn1c")


function(compile_asn1_lib)
    set(options)
    set(oneValueArgs )
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(AKTUALIZR_ASN1 "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # clean previously generated files
    set(ASN1_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/asn1/)
    file(MAKE_DIRECTORY ${ASN1_GEN_DIR})
    set(S)
    foreach(SA ${AKTUALIZR_ASN1_SOURCES})
        list(APPEND S ${CMAKE_CURRENT_SOURCE_DIR}/${SA})
    endforeach()
    execute_process(COMMAND ${ASN1C} ${ASN1C_FLAGS} ${S}
        WORKING_DIRECTORY ${ASN1_GEN_DIR}
        OUTPUT_QUIET
        )

    file(GLOB ASN1_GENERATED ${ASN1_GEN_DIR}/*.c)

    add_custom_command(
        OUTPUT ${ASN1_GENERATED}
        COMMAND ${ASN1C} ${ASN1C_FLAGS} ${S}
        WORKING_DIRECTORY ${ASN1_GEN_DIR}
        DEPENDS ${S}
        )

    list(REMOVE_ITEM ASN1_GENERATED ${ASN1_GEN_DIR}/converter-example.c ${ASN1_GEN_DIR}/converter-sample.c)

    add_library(asn1_lib OBJECT ${ASN1_GENERATED})

    target_include_directories(asn1_lib
        PUBLIC ${ASN1_GEN_DIR})
    include_directories(${ASN1_GEN_DIR})
    target_compile_options(asn1_lib
        PRIVATE "-Wno-error"
        PRIVATE "-w")
endfunction()
