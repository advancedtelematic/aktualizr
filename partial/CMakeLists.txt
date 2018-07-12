cmake_minimum_required(VERSION 2.8)

if(LIBUPTINY_MACHINE)
	if(${LIBUPTINY_MACHINE} STREQUAL "kea128")
		find_path(
			NXP_TOOLCHAIN_PATH
			NAMES Cross_Tools/gcc-arm-none-eabi-4_9/bin/arm-none-eabi-gcc
			HINTS $ENV{HOME}/NXP/S32DS_ARM_v1.3
			)
		message("Using NXP Toolchain in ${NXP_TOOLCHAIN_PATH}")

		# Cross-compilation options
		set(CMAKE_SYSTEM_NAME Generic)
		SET(CMAKE_CROSSCOMPILING 1)
		set(CMAKE_C_COMPILER_WORKS 1)
		set(CMAKE_SYSTEM_PROCESSOR cortex-m0plus)
		set(CMAKE_C_COMPILER ${NXP_TOOLCHAIN_PATH}/Cross_Tools/gcc-arm-none-eabi-4_9/bin/arm-none-eabi-gcc)
		set(CMAKE_ASM_COMPILER ${NXP_TOOLCHAIN_PATH}/Cross_Tools/gcc-arm-none-eabi-4_9/bin/arm-none-eabi-gcc)

		set(KEA128LIB_PREFIX machine/kea128/drivers)
		set(KEA128LIB_SOURCES ${KEA128LIB_PREFIX}/src/system_SKEAZ1284.c
			${KEA128LIB_PREFIX}/src/systimer.c
			${KEA128LIB_PREFIX}/src/led.c
			${KEA128LIB_PREFIX}/src/headlight.c
			${KEA128LIB_PREFIX}/src/can.c
			${KEA128LIB_PREFIX}/src/flash.c
			)

		include_directories(include ${NXP_TOOLCHAIN_PATH}/S32DS/arm_ewl2/EWL_C/include ${KEA128LIB_PREFIX}/include machine/kea128/app)

		set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/machine/kea128/SKEAZ_flash.ld")
                set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${LINKER_SCRIPT} -Xlinker --gc-sections -Xlinker -Map=kea128.map")
		set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)

		if(NOT DEFINED UPTANE_ECU_SERIAL)
			set(UPTANE_ECU_SERIAL 'TrudyGeneBailComradeYellowCone')
		endif()
		if(NOT DEFINED CAN_ID)
			set(CAN_ID "01")
		endif()
		if(NOT DEFINED UPTANE_HARDWARE_ID)
			set(UPTANE_HARDWARE_ID 'kea128')
		endif()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__START=__thumb_startup -DCLOCK_SETUP=1 -DCAN_ID=${CAN_ID} -DUPTANE_HARDWARE_ID=\\\"${UPTANE_HARDWARE_ID}\\\" -DUPTANE_ECU_SERIAL=\\\"${UPTANE_ECU_SERIAL}\\\" -DBYTE_ORDER_LITTLE -march=armv6-m -mtune=cortex-m0plus -mthumb --sysroot=${NXP_TOOLCHAIN_PATH}/S32DS/arm_ewl2 -specs=ewl_c_noio.specs -g -Os -std=c99 -Wno-main -ffunction-sections -fdata-sections")
		set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -x assembler-with-cpp -D__START=__thumb_startup -Os -march=armv6-m -mtune=cortex-m0plus -mthumb -ffunction-sections -fdata-sections --sysroot=${NXP_TOOLCHAIN_PATH}/S32DS/arm_ewl2 -specs=ewl_c_noio.specs")

		add_library(kea128_lib ${KEA128LIB_SOURCES})
		add_executable(kea128_ms1.elf machine/kea128/app/ms1.c machine/kea128/app/flash_load.c machine/kea128/app/uds.c machine/kea128/app/isotp_allocate.c machine/kea128/app/script.c machine/kea128/app/example_session.c machine/kea128/app/script.c machine/kea128/app/isotp_dispatch.c  machine/kea128/startup/startup_SKEAZ1284.S)
		target_link_libraries(kea128_ms1.elf kea128_lib)
		target_link_libraries(kea128_ms1.elf isotp_lib)

	endif()
endif()

set(ED25519_SOURCES ed25519/ed25519.c
	ed25519/edsign.c
	ed25519/f25519.c
	ed25519/fprime.c
	ed25519/sha512.c
	)

set(ED25519_HEADERS ed25519/ed25519.h
	ed25519/edsign.h
	ed25519/f25519.h
	ed25519/fprime.h
	ed25519/sha512.h
	)

set(UPTANE_SOURCES libuptiny/base64.c
	libuptiny/crypto.c
	libuptiny/root_signed.c
	libuptiny/signatures.c
	libuptiny/uptane_time.c
	libuptiny/utils.c
	extern/jsmn/jsmn.c
	)

set(UPTANE_HEADERS libuptiny/base64.h
	libuptiny/crypto.h
	libuptiny/root_signed.h
	libuptiny/signatures.h
	libuptiny/uptane_time.h
	libuptiny/utils.h
	extern/jsmn/jsmn.h
	)

set(LIBRARY_SOURCES ${ED25519_SOURCES} ${UPTANE_SOURCES})
set(LIBRARY_HEADERS ${ED25519_HEADERS} ${UPTANE_HEADERS})

include_directories(. ed25519 libuptiny extern)
add_library(uptane_partial_lib STATIC ${LIBRARY_SOURCES})
target_compile_options(uptane_partial_lib PUBLIC -Os -g -Wpedantic -Wno-long-long -fno-inline-small-functions -fno-inline-functions-called-once)

include_directories(extern/isotp-c/src extern/isotp-c/deps/bitfield-c/src/)
set(ISOTP_SOURCES extern/isotp-c/src/isotp/isotp.c
	extern/isotp-c/src/isotp/isotp.c
	extern/isotp-c/src/isotp/receive.c
	extern/isotp-c/src/isotp/send.c
	extern/isotp-c/deps/bitfield-c/src/bitfield/8byte.c
	extern/isotp-c/deps/bitfield-c/src/bitfield/bitarray.c
	extern/isotp-c/deps/bitfield-c/src/bitfield/bitfield.c
	)
add_library(isotp_lib STATIC ${ISOTP_SOURCES})
target_compile_options(isotp_lib PUBLIC -Os -g -Wno-long-long -fno-inline-small-functions -fno-inline-functions-called-once)

# Machine is not set, building tests to run in Linux environment
if(NOT LIBUPTINY_MACHINE)
	enable_testing()

	#add_executable(verify_targets examples/verify_targets.c)
	#target_link_libraries(verify_targets uptane_partial_lib)
	#target_compile_options(verify_targets PUBLIC -O0 -g)
	#set_target_properties(verify_targets PROPERTIES LINK_FLAGS "-Xlinker -Map=verify_targets.map")
	
        add_aktualizr_test(NAME tiny_base64 SOURCES libuptiny/base64.c tests/base64_test.cc)
        add_aktualizr_test(NAME tiny_signatures SOURCES libuptiny/base64.c libuptiny/uptane_time.c libuptiny/json_common.c libuptiny/signatures.c libuptiny/utils.c libuptiny/crypto.c extern/jsmn/jsmn.c ${ED25519_SOURCES} tests/test_state.cc tests/signatures_test.cc tests/test_common_data.cc)
        add_aktualizr_test(NAME tiny_signed_root SOURCES libuptiny/base64.c libuptiny/uptane_time.c libuptiny/json_common.c libuptiny/root_signed.c libuptiny/utils.c libuptiny/crypto.c extern/jsmn/jsmn.c ${ED25519_SOURCES} tests/test_state.cc tests/root_signed_test.cc tests/test_common_data.cc)
        add_aktualizr_test(NAME tiny_targets SOURCES libuptiny/base64.c libuptiny/uptane_time.c libuptiny/json_common.c libuptiny/signatures.c libuptiny/targets.c libuptiny/utils.c libuptiny/crypto.c extern/jsmn/jsmn.c ${ED25519_SOURCES} tests/test_state.cc tests/targets_test.cc tests/test_common_data.cc)
	add_executable(genpair ${ED25519_SOURCES} examples/genpair.c)
	add_executable(sign ${ED25519_SOURCES} examples/sign.c)

	set(test_exec ${CMAKE_CURRENT_SOURCE_DIR}/tests/targets/test.sh)
	add_test(NAME test_valid COMMAND ${test_exec} pass ${CMAKE_CURRENT_SOURCE_DIR}/tests/targets/data.json)
	add_test(NAME test_invalid COMMAND ${test_exec} fail ${CMAKE_CURRENT_SOURCE_DIR}/tests/targets/data.json)

	add_executable(test_flash_load machine/kea128/app/flash_load.c machine/kea128/tests/test_flash_load.c)
	target_include_directories(test_flash_load PUBLIC machine/kea128/drivers/include machine/kea128/app)
	target_compile_options(test_flash_load PUBLIC -m32)
	set_target_properties(test_flash_load PROPERTIES LINK_FLAGS  "-m32")
	add_test(NAME test_flash_load COMMAND test_flash_load)
endif()
