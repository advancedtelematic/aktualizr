add_custom_target(uptane_repo_full_no_correlation_id
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/full_no_correlation_id.sh
    ${UPTANE_GENERATOR} ${PROJECT_BINARY_DIR}/uptane_repos/full_no_correlation_id)
add_dependencies(uptane_repo_full_no_correlation_id uptane-generator)
