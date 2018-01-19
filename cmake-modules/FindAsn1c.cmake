find_program(ASN1C NAMES asn1c)

if(ASN1C MATCHES ".*-NOTFOUND")
    message(WARNING "asn1c not found")
endif(ASN1C MATCHES ".*-NOTFOUND")

if(ASN1C)
    function(add_asn1c_lib lib_name)
        set(ASN1_SRCS ${ARGN})
        foreach(MOD ${ASN1_SRCS})
            list(APPEND ASN1_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.asn1)
        endforeach()

        # compute the list of modules (ugly)...
        execute_process(COMMAND ${ASN1C} -pdu=all -P ${ASN1_FILES} OUTPUT_VARIABLE ASN1_ALL_OUTPUT)
        string(REGEX MATCHALL
            "/\\*\\*\\* <<< TYPE-DECLS \\[[A-Za-z_]+\\] >>> \\*\\*\\*/"
            ASN1_MODULES
            ${ASN1_ALL_OUTPUT}
            )
        string(REGEX REPLACE
            "/\\*\\*\\* <<< TYPE-DECLS \\[([A-Za-z_]+)\\] >>> \\*\\*\\*/"
            "\\1;"
            ASN1_MODULES
            ${ASN1_MODULES}
            )
        foreach(MOD ${ASN1_MODULES})
            list(APPEND ASN1_CFILES ${CMAKE_CURRENT_BINARY_DIR}/${MOD}.c)
        endforeach()

        set(ASN1_COMMON
            ${CMAKE_CURRENT_BINARY_DIR}/BOOLEAN.c
            ${CMAKE_CURRENT_BINARY_DIR}/INTEGER.c
            ${CMAKE_CURRENT_BINARY_DIR}/NativeEnumerated.c
            ${CMAKE_CURRENT_BINARY_DIR}/NativeInteger.c
            ${CMAKE_CURRENT_BINARY_DIR}/VisibleString.c
            ${CMAKE_CURRENT_BINARY_DIR}/asn_SEQUENCE_OF.c
            ${CMAKE_CURRENT_BINARY_DIR}/asn_SET_OF.c
            ${CMAKE_CURRENT_BINARY_DIR}/constr_CHOICE.c
            ${CMAKE_CURRENT_BINARY_DIR}/constr_SEQUENCE.c
            ${CMAKE_CURRENT_BINARY_DIR}/constr_SEQUENCE_OF.c
            ${CMAKE_CURRENT_BINARY_DIR}/constr_SET_OF.c
            ${CMAKE_CURRENT_BINARY_DIR}/OCTET_STRING.c
            ${CMAKE_CURRENT_BINARY_DIR}/BIT_STRING.c
            ${CMAKE_CURRENT_BINARY_DIR}/asn_codecs_prim.c
            ${CMAKE_CURRENT_BINARY_DIR}/ber_tlv_length.c
            ${CMAKE_CURRENT_BINARY_DIR}/ber_tlv_tag.c
            ${CMAKE_CURRENT_BINARY_DIR}/ber_decoder.c
            ${CMAKE_CURRENT_BINARY_DIR}/der_encoder.c
            ${CMAKE_CURRENT_BINARY_DIR}/constr_TYPE.c
            ${CMAKE_CURRENT_BINARY_DIR}/constraints.c
            ${CMAKE_CURRENT_BINARY_DIR}/xer_support.c
            ${CMAKE_CURRENT_BINARY_DIR}/xer_decoder.c
            ${CMAKE_CURRENT_BINARY_DIR}/xer_encoder.c
            ${CMAKE_CURRENT_BINARY_DIR}/per_support.c
            ${CMAKE_CURRENT_BINARY_DIR}/per_decoder.c
            ${CMAKE_CURRENT_BINARY_DIR}/per_encoder.c
            ${CMAKE_CURRENT_BINARY_DIR}/per_opentype.c
            )

        add_custom_command(
            OUTPUT ${ASN1_CFILES} ${ASN1_COMMON}
            COMMAND ${ASN1C} -pdu=all ${ASN1_FILES}
            DEPENDS ${ASN1_FILES}
            )

        # hardcoded list of common c files
        add_library(${lib_name}_asn1 STATIC
            ${ASN1_CFILES}
            ${ASN1_COMMON}
            )
        target_include_directories(${lib_name}_asn1
            PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
        target_compile_options(${lib_name}_asn1
            PRIVATE "-Wno-error"
            PRIVATE "-w")
    endfunction()
endif(ASN1C)
