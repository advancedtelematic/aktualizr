
set (GARAGE_PUSH_SRCS
  main.cc
  ostree_ref.cc
  ostree_repo.cc
  ostree_object.cc
  treehub_server.cc)

add_executable(garage-push ${GARAGE_PUSH_SRCS})
target_link_libraries(garage-push ${CURL_LIBRARIES} ${Boost_LIBRARIES})
# set_property(TARGET garage-push PROPERTY CXX_STANDARD 11)

set (ALL_SRCS
  ${GARAGE_PUSH_SRCS})

# For clang-format
set(ALL_HEADERS
  ostree_object.h
  ostree_ref.h
  ostree_repo.h
  treehub_server.h
)

# Export compile_commands.json for clang-check
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CLANG_FORMAT)
add_custom_target(format-tools
    COMMAND ${CLANG_FORMAT} -i -style Google ${ALL_SRCS} ${ALL_HEADERS}
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-format"
    VERBATIM)
add_dependencies(qa format-tools)
else()
    message(WARNING "clang-format not found, skipping")
endif()

add_custom_target(check-tools
    COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} ${ALL_SRCS}
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-check"
    VERBATIM)

add_dependencies(qa check-tools garage-push)

# Check the --help option works
add_test(NAME option-help
	COMMAND garage-push --help)

# Invalid arguments result in a failure
add_test(NAME bad-option
	COMMAND t/test-bad-option $<TARGET_FILE:garage-push>
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Detect that the thing we passed in to --repo is not a repository
add_test(NAME invalid-repo
  COMMAND t/test-invalid-repo $<TARGET_FILE:garage-push>
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Detect that the ref we passed in to --ref is not present
add_test(NAME missing-ref
  COMMAND t/test-missing-ref $<TARGET_FILE:garage-push>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME no-whitespace
  COMMAND t/test-no-whitespace ${ALL_SRCS} ${ALL_HEADERS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Workaround a cmake bug where tests are not dependent on their executables
# http://stackoverflow.com/questions/733475/cmake-ctest-make-test-doesnt-build-tests#736838
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND}
  DEPENDS garage-push)

add_dependencies(qa check)

install(TARGETS garage-push DESTINATION /usr/bin)

# vim: set tabstop=2 shiftwidth=2 expandtab:
