add_custom_target(uptane_repo_full_no_correlation_id
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/generate_repo.sh
    ${UPTANE_GENERATOR} ${PROJECT_BINARY_DIR}/uptane_repos/full_no_correlation_id --add_default_secondary)
add_dependencies(uptane_repo_full_no_correlation_id uptane-generator)
