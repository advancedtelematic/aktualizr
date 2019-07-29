if(BUILD_WITH_CODE_COVERAGE)
    add_definitions(${COVERAGE_COMPILER_FLAGS})
endif()

add_subdirectory("libaktualizr")
add_subdirectory("virtual_secondary")
add_subdirectory("sota_tools")
add_subdirectory("aktualizr_primary")
add_subdirectory("libaktualizr-posix")
add_subdirectory("aktualizr_secondary")
add_subdirectory("aktualizr_info")
add_subdirectory("aktualizr_repo")

add_subdirectory("cert_provider")
add_subdirectory("hmi_stub")
add_subdirectory("aktualizr_lite")
add_subdirectory("aktualizr_get")

add_subdirectory("load_tests")
