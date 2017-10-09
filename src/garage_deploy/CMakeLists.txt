set (GARAGE_DEPLOY_SRCS
  main.cc
)

add_executable(garage-deploy ${GARAGE_DEPLOY_SRCS})
target_link_libraries(garage-deploy ${Boost_LIBRARIES})

if(GARAGE_DEPLOY_VERSION)
    add_definitions(-DGARAGE_DEPLOY_VERSION="${GARAGE_DEPLOY_VERSION}")
else(GARAGE_DEPLOY_VERSION)
    add_definitions(-DGARAGE_DEPLOY_VERSION="dev")
endif(GARAGE_DEPLOY_VERSION)

# Export compile_commands.json for clang-check
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_target(check-format-garage-deploy
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/check-formatting.sh ${CLANG_FORMAT} ${GARAGE_DEPLOY_SRCS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking clang-format on src/garage_deploy"
    VERBATIM)
add_dependencies(check-format check-format-garage-deploy)

if(CLANG_FORMAT)
add_custom_target(format-garage-deploy
    COMMAND ${CLANG_FORMAT} -i -style=file ${GARAGE_DEPLOY_SRCS}
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-formaton src/garage_deploy"
    VERBATIM)
add_dependencies(format format-garage-deploy)
else()
    message(WARNING "clang-format not found, skipping")
endif()

if(CLANG_CHECK)
    add_custom_target(check-garage-deploy
        COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} ${GARAGE_DEPLOY_SRCS}
        WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-check"
        VERBATIM)
    add_dependencies(qa check-garage-deploy)
else()
    message(WARNING "clang-check not found, skipping")
endif(CLANG_CHECK)

INSTALL(TARGETS garage-deploy DESTINATION bin COMPONENT garage_deploy)

# vim: set tabstop=4 shiftwidth=4 expandtab: