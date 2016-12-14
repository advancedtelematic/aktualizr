#Setup CMake to run tests

# using boost test requires using static linking of libraries
set(Boost_USE_STATIC_LIBS ON)
# let cmake find the boost test library
find_package(Boost COMPONENTS unit_test_framework REQUIRED)

#enable test features of cmake
enable_testing()

# set include directories for test targets individually
set(TEST_INC_DIRS ${INC_DIR}
                  ${INC_DIR}/src)

# define the sources for the test                  
set(TEST_SRCS  ${SOURCES}
               test_src/test_servercon.cpp)

# set the name of the executable
add_executable(sota_client_test ${TEST_SRCS})

# indicates the include paths
target_include_directories(sota_client_test PRIVATE ${TEST_INC_DIRS})

# use the same libiraries as the normal target but add boost test
set (TEST_LIBS ${LINK_LIBS}
               ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
               
###############################################################################

# set the libraries for the current target               
target_link_libraries(sota_client_test ${TEST_LIBS})

unset(TEST_SRCS)
set(TEST_SRCS src/srv/servercon.cpp
              inc/srv/servercon.hpp
              inc/srv/oauthtoken.hpp
              src/logger.cpp
              inc/logger.hpp
              test_src/test_oauthtoken.cpp)
              
              # set the name of the executable
add_executable(sota_client_test_tokenValid ${TEST_SRCS})

# indicates the include paths
target_include_directories(sota_client_test_tokenValid PRIVATE ${TEST_INC_DIRS})

# set the libraries for the current target               
target_link_libraries(sota_client_test_tokenValid ${TEST_LIBS})

target_compile_definitions(sota_client_test_tokenValid PRIVATE TEST_ENABLE=1)

###############################################################################

# add the test target to coverage analysis
if(BUILD_WITH_CODE_COVERAGE)
target_link_libraries(sota_client_test gcov)
add_dependencies(coverage sota_client_test)
target_link_libraries(sota_client_test_tokenValid gcov)
add_dependencies(coverage sota_client_test_tokenValid)
endif(BUILD_WITH_CODE_COVERAGE)

# declares a test using the test target
add_test(NAME test_servercon COMMAND sota_client_test)


# declares a test using the test target
add_test(NAME test_oauthtoken COMMAND sota_client_test_tokenValid)

###############################################################################
# The test feature of cmake checks the return value when the program
# exits. If the return value is zero, the testcase passes.

# test running the executable with command line option --help
add_test(NAME feat1_test--help
         COMMAND sota_client --help -c ../config/config.yml.example)     
# test running the executable with command line option -h          
add_test(NAME feat1_test-h
         COMMAND sota_client -h -c ../config/config.yml.example)
# test running the executable with command line option --something
add_test(NAME feat1_test--something
         COMMAND sota_client --something -c ../config/config.yml.example)
# test running the executable with command line option -sth
add_test(NAME feat1_test-sth
         COMMAND sota_client -sth -c ../config/config.yml.example)
         
#test calling the executable with a config file         
add_test(NAME feat2_test_config_file
         COMMAND sota_client -c ../config/config.yml.example)
         
# calle the executable without any options
add_test(NAME test_plain
         COMMAND sota_client)

# check possible logging levels         
add_test(NAME test_log_trace
         COMMAND sota_client --loglevel=0 -c ../config/config.yml.example)
         
add_test(NAME test_log_debug
         COMMAND sota_client --loglevel=1 -c ../config/config.yml.example)
         
add_test(NAME test_log_warning
         COMMAND sota_client --loglevel=2 -c ../config/config.yml.example)
         
add_test(NAME test_log_info
         COMMAND sota_client --loglevel=3 -c ../config/config.yml.example)
         
add_test(NAME test_log_error
         COMMAND sota_client --loglevel=4 -c ../config/config.yml.example)

add_test(NAME test_log_invalid
         COMMAND sota_client --loglevel=5 -c ../config/config.yml.example)         
         
         
# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass         
set_tests_properties(feat1_test--something
                     feat1_test-sth
                     test_plain
                     PROPERTIES WILL_FAIL TRUE)
                     
# set the config file test to match the content of the config file as the config file
# contains a loglevel 1 (degub) where the content of the config file is logged to stderr 
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "thisisaclientid")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "thisisaclientsecret")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "https://url.com")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "Server: https://url.com")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "Loglevel: 1")
