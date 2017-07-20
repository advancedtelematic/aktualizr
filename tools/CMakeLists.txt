
set (GARAGE_PUSH_SRCS
  logging.cc
  main.cc
  oauth2.cc
  ostree_hash.cc
  ostree_ref.cc
  ostree_repo.cc
  ostree_object.cc
  request_pool.cc
  treehub_server.cc
)

add_executable(garage-push ${GARAGE_PUSH_SRCS})
target_link_libraries(garage-push ${CURL_LIBRARIES} ${GLIB2_LIBRARIES} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${LibArchive_LIBRARIES})

set (ALL_SRCS
  ${GARAGE_PUSH_SRCS})

# For clang-format
set(ALL_HEADERS
  accumulator.h
  logging.h
  oauth2.h
  ostree_hash.h
  ostree_object.h
  ostree_ref.h
  ostree_repo.h
  request_pool.h
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

# Detect a --repo that isn't in 'archive-z2' mode
add_test(NAME bare-mode-repo
        COMMAND t/test-bare-mode-repo $<TARGET_FILE:garage-push>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Detect that the ref we passed in to --ref is not present
add_test(NAME missing-ref
  COMMAND t/test-missing-ref $<TARGET_FILE:garage-push>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME no-whitespace
  COMMAND t/test-no-whitespace ${ALL_SRCS} ${ALL_HEADERS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME dry-run
  COMMAND t/test-dry-run $<TARGET_FILE:garage-push>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME cacert-not-found
  COMMAND t/test-cacert-not-found $<TARGET_FILE:garage-push>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME server-500
    COMMAND ./test-server-500 $<TARGET_FILE:garage-push>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/t)

if (OAUTH2_TEST_CLIENT_ID)
  add_test(NAME auth-plus-happy-case
    COMMAND t/test-auth-plus-happy $<TARGET_FILE:garage-push> ${OAUTH2_TEST_CLIENT_ID} ${OAUTH2_TEST_CLIENT_SECRET}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  add_test(NAME auth-plus-failure
    COMMAND t/test-auth-plus-failure $<TARGET_FILE:garage-push> ${OAUTH2_TEST_CLIENT_ID} ${OAUTH2_TEST_CLIENT_SECRET}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  add_test(NAME verbose-logging
    COMMAND t/test-verbose-logging $<TARGET_FILE:garage-push> ${OAUTH2_TEST_CLIENT_ID} ${OAUTH2_TEST_CLIENT_SECRET}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  if(STRACE)
  add_test(NAME cacert-used
          COMMAND t/test-cacert-used $<TARGET_FILE:garage-push> ${OAUTH2_TEST_CLIENT_ID} ${OAUTH2_TEST_CLIENT_SECRET} ${STRACE}
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
endif()

# Workaround a cmake bug where tests are not dependent on their executables
# http://stackoverflow.com/questions/733475/cmake-ctest-make-test-doesnt-build-tests#736838
add_custom_target(check COMMAND CTEST_OUTPUT_ON_FAILURE=1 ${CMAKE_CTEST_COMMAND}
  DEPENDS garage-push)

add_dependencies(qa check)

install(TARGETS garage-push DESTINATION /usr/bin)

# vim: set tabstop=2 shiftwidth=2 expandtab:
