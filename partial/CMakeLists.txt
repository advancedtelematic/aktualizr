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

set(C25519_SOURCES ed25519/ed25519.c
	ed25519/edsign.c
	ed25519/f25519.c
	ed25519/fprime.c
	ed25519/sha512.c
	)

set(C25519_HEADERS ed25519/ed25519.h
	ed25519/edsign.h
	ed25519/f25519.h
	ed25519/fprime.h
	ed25519/sha512.h
	)

set(UPTANE_SOURCES uptane/crypto.c
	uptane/targets.c
	uptane/uptane_time.c
	)

set(UPTANE_HEADERS uptane/crypto.h
	uptane/targets.h
	uptane/uptane_time.h
	)

set(LIBRARY_SOURCES ${C25519_SOURCES} ${UPTANE_SOURCES})
set(LIBRARY_HEADERS ${C25519_HEADERS} ${UPTANE_HEADERS})

include_directories(. ed25519 uptane)
add_library(uptane_partial_lib STATIC ${LIBRARY_SOURCES})

# Machine is not set, building tests to run in Linux environment
if(NOT MACHINE)
	add_executable(verify_targets examples/verify_targets.c)
	target_link_libraries(verify_targets uptane_partial_lib)
	target_compile_options(verify_targets PUBLIC -Os)
	target_compile_options(uptane_partial_lib PUBLIC -Os)
	set_target_properties(verify_targets PROPERTIES LINK_FLAGS "-Xlinker -Map=verify_targets.map")
	
	add_executable(genpair ${C25519_SOURCES} examples/genpair.c)
	add_executable(sign ${C25519_SOURCES} examples/sign.c)

endif()
