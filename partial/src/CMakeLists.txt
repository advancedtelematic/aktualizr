cmake_minimum_required(VERSION 2.8)

project(uptane-partial)
if(MACHINE)
	if(${MACHINE} EQUAL "kea128")
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

		include_directories(include ${NXP_TOOLCHAIN_PATH}/S32DS/arm_ewl2/EWL_C/include)
	endif()
endif()

set(LIBRARY_SOURCES crypto.c
	targets.c
	uptane_time.c
	ed25519/ed25519.c
	ed25519/edsign.c
	ed25519/f25519.c
	ed25519/fprime.c
	ed25519/sha512.c)

include_directories(. ed25519)
add_library(uptane_partial_lib STATIC ${LIBRARY_SOURCES})

# Machine is not set, building tests to run in Linux environment
if(NOT MACHINE)
	add_executable(verify_targets examples/verify_targets.c)
	target_link_libraries(verify_targets uptane_partial_lib)
endif()
