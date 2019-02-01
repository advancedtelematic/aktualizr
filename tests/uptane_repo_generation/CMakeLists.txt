add_custom_target(uptane_repo_full_no_correlation_id
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/full_no_correlation_id.sh
    $<TARGET_FILE:aktualizr-repo> ${PROJECT_BINARY_DIR}/uptane_repos/full_no_correlation_id)
add_dependencies(uptane_repo_full_no_correlation_id aktualizr-repo)

add_custom_target(uptane_repo_delegation_basic
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/delegation_basic.sh
    $<TARGET_FILE:aktualizr-repo> ${PROJECT_BINARY_DIR}/uptane_repos/delegation_basic)
add_dependencies(uptane_repo_delegation_basic aktualizr-repo)
