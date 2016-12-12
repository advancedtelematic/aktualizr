#Setup CMake to run tests

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS unit_test_framework REQUIRED)

enable_testing()

      
set(TEST_LIBS ${LINK_LIBS}
            libboost_test.so)


# indicates the location of the boost installation tree.
# hard-coded for our simple example.
set(BOOST_INCLUDE_DIRS usr/include)

set(TEST_INC_DIRS ${BOOST_INCLUDE_DIRS}
                  ${INC_DIR}
                  ${INC_DIR}/src)
                  
set(TEST_SRCS  ${SOURCES}
               test_src/test_servercon.cpp)

# set the name of the executable
add_executable(sota_client_test ${TEST_SRCS})

# indicates the include paths
target_include_directories(sota_client_test PRIVATE ${TEST_INC_DIRS})

set (TEST_LIBS ${LINK_LIBS}
               ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
               
target_link_libraries(sota_client_test ${TEST_LIBS})


# declares a test with our executable
add_test(NAME test_servercon COMMAND sota_client_test)

###############################################################################
# The test feature of cmake checks the return value when the program
# exits. If the return value is zero, the testcase passes.

# test running the executable with command line option --help
add_test(NAME feat1_test--help
         COMMAND sota_client --help)     
# test running the executable with command line option -h          
add_test(NAME feat1_test-h
         COMMAND sota_client -h)
# test running the executable with command line option --something
add_test(NAME feat1_test--something
         COMMAND sota_client --something)
# test running the executable with command line option -sth
add_test(NAME feat1_test-sth
         COMMAND sota_client -sth)
# test running the executable without command line options
add_test(NAME feat1_test-no_opt
         COMMAND sota_client)
         
#test calling the executable with a config file         
add_test(NAME feat2_test_config_file
         COMMAND sota_client -c ../config/config.yml.example)
         
# calle the executable without any options
add_test(NAME test_plain
         COMMAND sota_client)

# check possible logging levels         
add_test(NAME test_log_trace
         COMMAND sota_client --loglevel=0)
         
add_test(NAME test_log_debug
         COMMAND sota_client --loglevel=1)
         
add_test(NAME test_log_warning
         COMMAND sota_client --loglevel=2)
         
add_test(NAME test_log_info
         COMMAND sota_client --loglevel=3)
         
add_test(NAME test_log_error
         COMMAND sota_client --loglevel=4)

add_test(NAME test_log_invalid
         COMMAND sota_client --loglevel=5)         
         
         
# testcases with non-supported command line options will return 1
# in this case we want the testcase to pass         
set_tests_properties(feat1_test--something
                     feat1_test-sth 
                     PROPERTIES WILL_FAIL TRUE)
                     
# set the config file test to match the content of the config file as the config file
# contains a loglevel 1 (degub) where the content of the config file is logged to stderr 
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "thisisaclientid")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "thisisaclientsecret")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "https://url.com")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "Server: https://url.com")
set_tests_properties(feat2_test_config_file PROPERTIES PASS_REGULAR_EXPRESSION "Loglevel: 1")
