find_program(ASN1C NAMES asn1c)

if(ASN1C MATCHES ".*-NOTFOUND")
    message(WARNING "asn1c not found")
endif(ASN1C MATCHES ".*-NOTFOUND")

if(ASN1C)
    message(STATUS "Found asn1c: ${ASN1C}")

    # -fnative-types is required for compat with asn1c <= 0.9.24
    set(ASN1C_FLAGS ${ASN1C_FLAGS} -fskeletons-copy -fnative-types -pdu=all)

    function(add_asn1c_lib lib_name)
        set(ASN1_SRCS ${ARGN})
        foreach(MOD ${ASN1_SRCS})
            list(APPEND ASN1_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${MOD}.asn1)
        endforeach()

        # clean previously generated files
        set(ASN1_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/asn1)
        if(IS_DIRECTORY ${ASN1_GEN_DIR})
            file(REMOVE_RECURSE ${ASN1_GEN_DIR})
        elseif(EXISTS ${ASN1_GEN_DIR})
            message(FATAL_ERROR "${ASN1_GEN_DIR} not a directory")
        endif(IS_DIRECTORY ${ASN1_GEN_DIR})
        file(MAKE_DIRECTORY ${ASN1_GEN_DIR})

        execute_process(COMMAND ${ASN1C} ${ASN1C_FLAGS} ${ASN1_FILES}
            WORKING_DIRECTORY ${ASN1_GEN_DIR}
            OUTPUT_QUIET
            )
        file(GLOB ASN1_GENERATED ${ASN1_GEN_DIR}/*.c)

        add_custom_command(
            OUTPUT ${ASN1_GENERATED}
            COMMAND ${ASN1C} ${ASN1C_FLAGS} ${ASN1_FILES}
            WORKING_DIRECTORY ${ASN1_GEN_DIR}
            DEPENDS ${ASN1_FILES}
            )

        # hardcoded list of common c files
        add_library(${lib_name}_asn1 STATIC ${ASN1_GENERATED})

        target_include_directories(${lib_name}_asn1
            PUBLIC ${ASN1_GEN_DIR})
        target_compile_options(${lib_name}_asn1
            PRIVATE "-Wno-error"
            PRIVATE "-w")
    endfunction()
endif(ASN1C)
