option(ENABLE_FUZZING "Create executables and targets for fuzzing aktualizr with afl." Off)
if (ENABLE_FUZZING)
    find_program(AFL_FUZZ afl-fuzz)
    if ("${AFL_FUZZ}" MATCHES "AFL_FUZZ-NOTFOUND")
        message(FATAL_ERROR "Couldn't find afl-fuzz.")
    endif()

    add_executable(afl afl.cc)
    target_link_libraries(afl aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS})

    if (NOT ENABLE_SANITIZERS)
        message(FATAL_ERROR "Enable sanitizers with -DENABLE_SANITIZERS=On to do fuzzing.")
    endif()

    add_custom_target(fuzz
        COMMAND "AFL_SKIP_CPUFREQ=1" "${AFL_FUZZ}" -i "${CMAKE_CURRENT_SOURCE_DIR}/input" -o "${CMAKE_CURRENT_SOURCE_DIR}/output" -m 200 -- "${CMAKE_CURRENT_BINARY_DIR}/afl" "@@"
        DEPENDS afl)
endif()
