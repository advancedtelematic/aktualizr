cmake_minimum_required (VERSION 3.5)
# CMAKE_EXPORT_COMPILE_COMMANDS requires 3.5
# CMAKE_CXX_STANDARD and CMAKE_CXX_EXTENSIONS require 3.1.3

# GLOB_RECURSE calls should not follow symlinks by default:
cmake_policy(SET CMP0009 NEW)

project(aktualizr)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "No CMAKE_BUILD_TYPE specified, defaulting to ${CMAKE_BUILD_TYPE}")
endif(NOT CMAKE_BUILD_TYPE)

if(CMAKE_BUILD_TYPE MATCHES "Valgrind")
    set(TESTSUITE_VALGRIND_DEFAULT ON)
else()
    set(TESTSUITE_VALGRIND_DEFAULT OFF)
endif()

option(WARNING_AS_ERROR "Treat warnings as errors" ON)
option(PEDANTIC_WARNINGS "Compile with pedantic warnings" OFF)
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)
option(BUILD_OSTREE "Set to ON to compile with OSTree support" OFF)
option(BUILD_DEB "Set to ON to compile with debian packages support" OFF)
option(BUILD_P11 "Support for key storage in a HSM via PKCS#11" OFF)
option(BUILD_SOTA_TOOLS "Set to ON to build SOTA tools" OFF)
option(FAULT_INJECTION "Set to ON to enable fault injection" OFF)
option(INSTALL_LIB "Set to ON to install library and headers" OFF)
option(TESTSUITE_VALGRIND "Set to ON to make tests to run under valgrind (default when CMAKE_BUILD_TYPE=Valgrind)" ${TESTSUITE_VALGRIND_DEFAULT})
option(CCACHE "Set to ON to use ccache if available" ON)

set(SOTA_PACKED_CREDENTIALS "" CACHE STRING "Credentials.zip for tests involving the server")

set(TESTSUITE_ONLY "" CACHE STRING "Only run tests matching this list of labels")
set(TESTSUITE_EXCLUDE "" CACHE STRING "Exclude tests matching this list of labels")

set(STORAGE_TYPE "sqlite" CACHE STRING "")

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "Aktualizr does not support building in the source tree. Please remove CMakeCache.txt and the CMakeFiles/ directory, then create a subdirectory to build in: mkdir build; cd build; cmake ..")
endif()

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules)
configure_file(CTestCustom.cmake CTestCustom.cmake)

unset(AKTUALIZR_CHECKED_SRCS CACHE)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# To ensure better support of large files on 32-bit systems.
# See https://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html
add_definitions("-D_FILE_OFFSET_BITS=64")

if (CCACHE)
    find_program(CCACHE_PROGRAM ccache)
    if (CCACHE_PROGRAM)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    endif (CCACHE_PROGRAM)
endif (CCACHE)

# find all required libraries
set(BOOST_COMPONENTS log_setup log system filesystem program_options)
set(Boost_USE_STATIC_LIBS OFF)
add_definitions(-DBOOST_LOG_DYN_LINK)

# Mac brew library install paths
if(EXISTS /usr/local/opt/openssl)
  list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/openssl)
endif()

if(EXISTS /usr/local/opt/libarchive)
  list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/libarchive)
endif()

find_package(Boost 1.58.0 COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL 1.0.2 REQUIRED)
find_package(Threads REQUIRED)
find_package(LibArchive REQUIRED)
find_package(sodium REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Git)
find_package(Asn1c REQUIRED)

if(NOT AKTUALIZR_VERSION)
    if (EXISTS ${PROJECT_SOURCE_DIR}/VERSION)
        file(READ ${PROJECT_SOURCE_DIR}/VERSION AKTUALIZR_VERSION)
        string(STRIP "${AKTUALIZR_VERSION}" AKTUALIZR_VERSION)
    elseif (GIT_EXECUTABLE)
        execute_process(COMMAND ${PROJECT_SOURCE_DIR}/scripts/get_version.sh ${GIT_EXECUTABLE} ${PROJECT_SOURCE_DIR} OUTPUT_VARIABLE AKTUALIZR_VERSION RESULT_VARIABLE RES)
        if (NOT RES EQUAL 0)
            message(FATAL_ERROR "Could not get current version from git")
        endif ()
    endif ()
    if (NOT AKTUALIZR_VERSION)
        message(FATAL_ERROR "Version was not detected properly, verify that you have either a git checkout or a VERSION file present in aktualizr's main directory")
    endif ()
    message(STATUS "Setting version to ${AKTUALIZR_VERSION}")
endif(NOT AKTUALIZR_VERSION)

if(BUILD_OSTREE)
    find_package(OSTree REQUIRED)
    add_definitions(-DBUILD_OSTREE)
else(BUILD_OSTREE)
    unset(LIBOSTREE_LIBRARIES CACHE)
endif(BUILD_OSTREE)

if(BUILD_P11)
    find_package(LibP11 REQUIRED)
    add_definitions(-DBUILD_P11)

    if (NOT PKCS11_ENGINE_PATH)
        if (CMAKE_CROSSCOMPILING)
            message(FATAL_ERROR "Please define the PKCS11_ENGINE_PATH on your target system")
        endif()
        foreach(engine IN ITEMS "/usr/lib/engines-1.1/pkcs11.so"
                "/usr/lib/engines/pkcs11.so"
                "/usr/lib/x86_64-linux-gnu/engines-1.1/pkcs11.so")

            if(EXISTS ${engine})
                message(STATUS "Detected pkcs11 engine path: ${engine}")
                set(PKCS11_ENGINE_PATH ${engine})
            endif()

        endforeach()

        if (NOT PKCS11_ENGINE_PATH)
            message(FATAL_ERROR "Could not auto-detect path of PKCS11 engine, please specify PKCS11_ENGINE_PATH")
        endif()

        set(PKCS11_ENGINE_PATH "${PKCS11_ENGINE_PATH}" CACHE STRING "Credentials.zip for tests involving the server")
    endif()
endif(BUILD_P11)

if(BUILD_SOTA_TOOLS)
    find_package(GLIB2 REQUIRED)
    find_program(STRACE NAMES strace)
endif(BUILD_SOTA_TOOLS)

if(FAULT_INJECTION)
    find_package(Libfiu REQUIRED)
    add_definitions(-DFIU_ENABLE)
    link_libraries(fiu dl)
    install(PROGRAMS scripts/fiu DESTINATION bin COMPONENT aktualizr)
endif(FAULT_INJECTION)

# flags for different build types
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")
set(CMAKE_C_FLAGS_DEBUG "-Og -g")

SET(CMAKE_CXX_FLAGS_VALGRIND "-O1 -g")
SET(CMAKE_C_FLAGS_VALGRIND "-O1 -g")

SET(CMAKE_CXX_FLAGS_ASAN "-O1 -g -fsanitize=address -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer")
SET(CMAKE_C_FLAGS_ASAN "-O1 -g -fsanitize=address -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer")
SET(CMAKE_EXE_LINKER_FLAGS_ASAN "-O1 -g -fsanitize=address -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer")

SET(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer")
SET(CMAKE_C_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer")
SET(CMAKE_EXE_LINKER_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer")

if (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
    add_definitions(-fstack-protector-all)
    # Enable maximum set of warnings.
    add_definitions(-Wall -Wextra -Wformat-security -Wfloat-equal -Wcast-qual -Wswitch-default -Wconversion)
    # only for CXX, gcc doesn't like that when building C...
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor")
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_definitions(-Wlogical-op)
    else ()
        add_definitions(-Wno-unused-private-field)
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "4.9" OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.9")
        add_definitions(-Wshadow)
    endif ()

    if (WARNING_AS_ERROR)
        add_definitions(-Werror)
    endif ()

    if (PEDANTIC_WARNINGS)
        add_definitions(-Wpedantic)
    endif (PEDANTIC_WARNINGS)

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
    endif ()
endif()

# prevent visibility warnings from the linker
if (APPLE)
  link_libraries("-Wl,-w")
endif()

################ QA RULES
add_custom_target(qa)
add_custom_target(check-format)

add_custom_target(format)
add_dependencies(qa format)

# All binaries are built as prerequisites to this target
if(TESTSUITE_VALGRIND)
    find_program(VALGRIND NAMES valgrind)
    if(VALGRIND MATCHES ".*-NOTFOUND")
        set(VALGRIND valgrind)
    endif()
    configure_file(scripts/run-valgrind.in ${CMAKE_CURRENT_BINARY_DIR}/run-valgrind @ONLY)
    set(RUN_VALGRIND ${CMAKE_CURRENT_BINARY_DIR}/run-valgrind)
endif()
add_custom_target(build_tests)

# clang-check and clang-format
find_program(CLANG_FORMAT NAMES clang-format-10)
find_program(CLANG_TIDY NAMES clang-tidy-10)

if(CLANG_FORMAT)
    function(aktualizr_clang_format)
        file(RELATIVE_PATH SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(FILE ${ARGN})
            string(REPLACE "/" "_" TARGETNAME "aktualizr_clang_format-${SUBDIR}-${FILE}")
            add_custom_target(${TARGETNAME}
                COMMAND ${CLANG_FORMAT} -i -style=file ${FILE}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                VERBATIM)
            add_dependencies(format ${TARGETNAME})

            # The check for CI that fails if stuff changes
            string(REPLACE "/" "_" TARGETNAME_CI "aktualizr_ci_clang_format-${SUBDIR}-${FILE}")

            add_custom_target(${TARGETNAME_CI}
                COMMAND ${CLANG_FORMAT} -style=file ${FILE} | diff -u ${FILE} - || { echo 'Found unformatted code! Run make format'\; false\; }
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            add_dependencies(check-format ${TARGETNAME_CI})
        endforeach()
    endfunction()
else()
    message(WARNING "clang-format-10 not found, skipping")
    function(aktualizr_clang_format)
    endfunction()
endif()

if(CLANG_TIDY)
    add_custom_target(clang-tidy)
    add_dependencies(qa clang-tidy)
    function(aktualizr_clang_tidy)
        file(RELATIVE_PATH SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(FILE ${ARGN})
            # Skip header files, since they are ignored. Headers are checked if
            # they are included by a checked source file.
            if(${FILE} MATCHES "\\.h")
                continue()
            endif()
            string(REPLACE "/" "_" TARGETNAME "aktualizr_clang_tidy-${SUBDIR}-${FILE}")
            add_custom_target(${TARGETNAME}
                COMMAND ${PROJECT_SOURCE_DIR}/scripts/clang-tidy-wrapper.sh ${CLANG_TIDY} ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR} ${FILE}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                VERBATIM)
            add_dependencies(clang-tidy ${TARGETNAME})
        endforeach()
    endfunction()
else()
    message(WARNING "clang-tidy-10 not found, skipping")
    function(aktualizr_clang_tidy)
    endfunction()
endif()

function(aktualizr_source_file_checks)
    list(REMOVE_DUPLICATES ARGN)
    foreach(FILE ${ARGN})
        file(RELATIVE_PATH FULL_FN ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE})
        set(AKTUALIZR_CHECKED_SRCS ${AKTUALIZR_CHECKED_SRCS} ${FULL_FN} CACHE INTERNAL "")
        if(NOT EXISTS ${CMAKE_SOURCE_DIR}/${FULL_FN})
            message(FATAL_ERROR "file ${FULL_FN} does not exist")
        endif()
    endforeach()
    aktualizr_clang_format(${ARGN})

    # exclude test files from clang-tidy because false positives in googletest
    # are hard to remove...
    file(RELATIVE_PATH SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT ${SUBDIR} MATCHES "tests.*")
        foreach(FILE ${ARGN})
            if(NOT ${FILE} MATCHES ".*_test\\..*$")
                list(APPEND filtered_files ${FILE})
            endif()
        endforeach()
        aktualizr_clang_tidy(${filtered_files})
    endif()
endfunction()

# Use C++11, but without GNU or other extensions
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

############### BUILD RULES
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src/libaktualizr)
include_directories(${PROJECT_SOURCE_DIR}/third_party/googletest/googletest/include)
include_directories(${JSONCPP_INCLUDE_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
include_directories(${LIBOSTREE_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIRS})
include_directories(${LIBP11_INCLUDE_DIR})
include_directories(${sodium_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${CURL_INCLUDE_DIR})
include_directories(${LibArchive_INCLUDE_DIR})

# General packaging configuration
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Patrick Vacek <patrick.vacek@here.com>")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING ONE_PER_GROUP)
set(CPACK_DEBIAN_PACKAGE_VERSION ${AKTUALIZR_VERSION})
set(CPACK_COMPONENTS_ALL aktualizr)

if(BUILD_SOTA_TOOLS)
    set(CPACK_COMPONENTS_ALL ${CPACK_COMPONENTS_ALL} garage_deploy)
    set(CPACK_DEBIAN_GARAGE_DEPLOY_PACKAGE_NAME "garage-deploy")
    set(CPACK_DEBIAN_GARAGE_DEPLOY_FILE_NAME "garage_deploy.deb")  # only available for CMake >= 3.6.0
    set(CPACK_COMPONENT_GARAGE_DEPLOY_DESCRIPTION "garage-deploy utility")
    set(CPACK_DEBIAN_GARAGE_DEPLOY_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_GARAGE_DEPLOY_PACKAGE_DEPENDS "openjdk-8-jre")
    if(SOTA_DEBIAN_PACKAGE_DEPENDS)
      set(CPACK_DEBIAN_GARAGE_DEPLOY_PACKAGE_DEPENDS ${SOTA_DEBIAN_PACKAGE_DEPENDS})
    endif()
    message(STATUS "Using ${CPACK_DEBIAN_GARAGE_DEPLOY_PACKAGE_DEPENDS} as Debian package depends.")
endif(BUILD_SOTA_TOOLS)

set(CPACK_DEBIAN_AKTUALIZR_PACKAGE_NAME "aktualizr")
set(CPACK_DEBIAN_AKTUALIZR_FILE_NAME "aktualizr.deb")  # only available for CMake >= 3.6.0
set(CPACK_COMPONENT_AKTUALIZR_DESCRIPTION "Uptane-compliant embedded software update client\n Aktualizr communicates with OTA Connect,\n reports device information, and fetches and installs package updates")
set(CPACK_DEBIAN_AKTUALIZR_PACKAGE_DEPENDS "lshw")
set(CPACK_DEBIAN_AKTUALIZR_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_AKTUALIZR_PACKAGE_CONTROL_EXTRA "${PROJECT_SOURCE_DIR}/config/systemd/debian-control/postinst;${PROJECT_SOURCE_DIR}/config/systemd/debian-control/prerm;${PROJECT_SOURCE_DIR}/config/systemd/debian-control/postrm;")
include(CPack)
include(CPackComponent)


set (AKTUALIZR_EXTERNAL_LIBS
    ${Boost_SYSTEM_LIBRARIES}
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${sodium_LIBRARY_RELEASE}
    ${LIBOSTREE_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${LibArchive_LIBRARIES}
    ${LIBP11_LIBRARIES}
    ${GLIB2_LIBRARIES})

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set (AKTUALIZR_EXTERNAL_LIBS ${AKTUALIZR_EXTERNAL_LIBS} PARENT_SCOPE)
endif()

include(AddAktualizrTest)
set (TEST_LIBS gtest gmock testutilities aktualizr_lib)
if(BUILD_WITH_CODE_COVERAGE)
    set(COVERAGE_LCOV_EXCLUDES '/usr/include/*' ${CMAKE_BINARY_DIR}'*' ${CMAKE_SOURCE_DIR}'/third_party/*' ${CMAKE_SOURCE_DIR}'/tests/*' '*_test.cc')
    include(CodeCoverage)
    set(COVERAGE_COMPILER_FLAGS "--coverage -fprofile-arcs -ftest-coverage" CACHE INTERNAL "")
endif(BUILD_WITH_CODE_COVERAGE)

include(CTest)
ENABLE_TESTING()
# It would be great to use GTEST_OUTPUT directly, but I couldn't get it to work.
set(GOOGLE_TEST_OUTPUT --gtest_output=xml:${CMAKE_BINARY_DIR}/results/)

# amalgamate jsoncpp source at compile time
# note: jsoncpp has a CMake support that they intent to deprecate and is hard to
# integrate with.
execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/amalgamate-jsoncpp.sh ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jsoncpp ${CMAKE_CURRENT_BINARY_DIR}/jsoncpp)
include_directories(${PROJECT_BINARY_DIR}/jsoncpp)
# jsoncpp triggers a number of warnings that are turned on by default in our build
set_source_files_properties(${PROJECT_BINARY_DIR}/jsoncpp/jsoncpp.cc PROPERTIES
    COMPILE_FLAGS "-Wno-error -Wno-float-equal -Wno-switch-default")
add_library(jsoncpp OBJECT ${PROJECT_BINARY_DIR}/jsoncpp/jsoncpp.cc)

add_subdirectory("config")
add_subdirectory("src")
add_subdirectory("tests" EXCLUDE_FROM_ALL)
add_subdirectory("docs/doxygen")

# Check if some source files were not added sent to `aktualizr_source_file_checks`
#
# Note: does not check `tests` directory which depends too much on conditional
# compilation
file(GLOB_RECURSE ALL_SOURCE_FILES RELATIVE ${CMAKE_SOURCE_DIR}
    src/*.cc src/*.c src/*.h)
foreach(FILE ${ALL_SOURCE_FILES})
    list(FIND AKTUALIZR_CHECKED_SRCS ${FILE} INDEX)
    if (${INDEX} EQUAL "-1")
        message(FATAL_ERROR "${FILE} not checked")
    endif ()
endforeach()

# Generate ctags
set_source_files_properties(tags PROPERTIES GENERATED true)
add_custom_target(tags
    COMMAND ctags -R --c++-kinds=+p --fields=+iaS --extra=+q src
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# vim: set tabstop=4 shiftwidth=4 expandtab:
