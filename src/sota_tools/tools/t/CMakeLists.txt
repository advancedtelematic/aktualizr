# Ignore warning from gtest.
add_definitions (-Wno-switch-default)

if(NOT GTEST_ROOT )
    set(GTEST_ROOT ${PROJECT_SOURCE_DIR}/../../third_party/googletest/googletest)
endif()

if(NOT GMOCK_ROOT )
    set(GMOCK_ROOT ${PROJECT_SOURCE_DIR}/../../third_party/googletest/googlemock)
endif()
add_subdirectory(${GMOCK_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gmock EXCLUDE_FROM_ALL)

# use the same libraries as the normal target but add gtest gmock and gcov
set (TEST_LIBS
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${LibArchive_LIBRARIES}
    gtest gmock gcov)

add_custom_target(build_tests)

# Config Test
add_executable(garage_push_auth_test auth_test.cc)
target_link_libraries(garage_push_auth_test garage_push_static_lib ${TEST_LIBS})
add_test(NAME auth_test COMMAND garage_push_auth_test WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_dependencies(build_tests
    garage_push_auth_test)

set(ALL_TEST_SRCS
    auth_test.cc)

if(CLANG_FORMAT)
    add_custom_target(format-tests
        COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_TEST_SRCS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on tests"
        VERBATIM)
    add_dependencies(qa format-tests)
else()
    message(WARNING "clang-format not found, skipping")
endif()

# vim: set tabstop=4 shiftwidth=4 expandtab:

