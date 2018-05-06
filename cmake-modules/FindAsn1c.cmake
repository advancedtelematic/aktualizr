find_program(ASN1C NAMES asn1c)

if(ASN1C MATCHES ".*-NOTFOUND")
    message(WARNING "asn1c not found")
endif(ASN1C MATCHES ".*-NOTFOUND")

if(ASN1C)
    message(STATUS "Found asn1c: ${ASN1C}")

    # -fnative-types is required for compat with asn1c <= 0.9.24
    set(ASN1C_FLAGS ${ASN1C_FLAGS} -fskeletons-copy -fnative-types -pdu=all)

    define_property(GLOBAL PROPERTY ASN1_FILES_GLOBAL
                    BRIEF_DOCS "List of all input files for asn1c"
                    FULL_DOCS "List of all input files for asn1c")

    function(add_asn1c_lib)
        set(ASN1_SRCS ${ARGN})
        foreach(MOD ${ASN1_SRCS})
	    set_property(GLOBAL APPEND PROPERTY ASN1_FILES_GLOBAL ${MOD}.asn1)
        endforeach()
    endfunction()

    function(compile_asn1_lib)
	    get_property(ASN1_FILES GLOBAL PROPERTY ASN1_FILES_GLOBAL)
        # clean previously generated files
        set(ASN1_GEN_DIR ${PROJECT_SOURCE_DIR}/generated/asn1/)
        file(MAKE_DIRECTORY ${ASN1_GEN_DIR})

        execute_process(COMMAND ${ASN1C} ${ASN1C_FLAGS} ${ASN1_FILES}
            WORKING_DIRECTORY ${ASN1_GEN_DIR}
            OUTPUT_QUIET ERROR_QUIET
            )

        file(GLOB ASN1_GENERATED ${ASN1_GEN_DIR}/*.c)

        add_custom_command(
            OUTPUT ${ASN1_GENERATED}
            COMMAND ${ASN1C} ${ASN1C_FLAGS} ${ASN1_FILES}
            WORKING_DIRECTORY ${ASN1_GEN_DIR}
            DEPENDS ${ASN1_FILES}
            )

        list(REMOVE_ITEM ASN1_GENERATED ${ASN1_GEN_DIR}/converter-example.c ${ASN1_GEN_DIR}/converter-sample.c)

        # hardcoded list of common c files
        add_library(asn1_lib STATIC ${ASN1_GENERATED})

        target_include_directories(asn1_lib
            PUBLIC ${ASN1_GEN_DIR})
        target_compile_options(asn1_lib
            PRIVATE "-Wno-error"
            PRIVATE "-w")
    endfunction()
endif(ASN1C)
