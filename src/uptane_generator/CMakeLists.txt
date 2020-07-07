
set(UPTANE_GENERATOR_SRC repo.cc director_repo.cc image_repo.cc uptane_repo.cc)
set(UPTANE_GENERATOR_HDR repo.h director_repo.h image_repo.h uptane_repo.h)

set(UPTANE_GENERATOR_LIBS aktualizr_lib)
add_library(uptane_generator_lib ${UPTANE_GENERATOR_SRC})
target_include_directories(uptane_generator_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(uptane-generator main.cc ${UPTANE_GENERATOR_SRC})
target_link_libraries(uptane-generator ${UPTANE_GENERATOR_LIBS})

install(TARGETS uptane-generator
        COMPONENT aktualizr
        RUNTIME DESTINATION bin)

aktualizr_source_file_checks(${UPTANE_GENERATOR_SRC} ${UPTANE_GENERATOR_HDR})
add_aktualizr_test(NAME uptane_generator
                   SOURCES repo_test.cc
                   LIBRARIES uptane_generator_lib
                   ARGS $<TARGET_FILE:uptane-generator>
                   PROJECT_WORKING_DIRECTORY)

# Check the --help option works.
add_test(NAME uptane-generator-option-help
         COMMAND uptane-generator --help)

# Report version.
add_test(NAME uptane-generator-option-version
         COMMAND uptane-generator --version)
set_tests_properties(uptane-generator-option-version PROPERTIES PASS_REGULAR_EXPRESSION "Current uptane-generator version is: ${AKTUALIZR_VERSION}")

aktualizr_source_file_checks(${TEST_SOURCES} main.cc)

# vim: set tabstop=4 shiftwidth=4 expandtab:
