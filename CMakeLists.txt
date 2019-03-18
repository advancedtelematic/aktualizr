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

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set(BUILD_SYSTEMD_DEFAULT ON)
else()
    set(BUILD_SYSTEMD_DEFAULT OFF)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Valgrind")
    set(TESTSUITE_VALGRIND_DEFAULT ON)
else()
    set(TESTSUITE_VALGRIND_DEFAULT OFF)
endif()

option(WARNING_AS_ERROR "Treat warnings as errors" ON)
option(PEDANTIC_WARNINGS "Compile with pedantic warnings" OFF)
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)
option(BUILD_OSTREE "Set to ON to compile with ostree support" OFF)
option(BUILD_DEB "Set to ON to compile with debian packages support" OFF)
option(BUILD_P11 "Support for key storage in a HSM via PKCS#11" OFF)
option(BUILD_SOTA_TOOLS "Set to ON to build SOTA tools" OFF)
option(BUILD_OPCUA "Set to ON to compile with OPC-UA protocol support" OFF)
option(BUILD_PARTIAL "Set to ON to compile with partial secondaries support" OFF)
option(BUILD_ISOTP "Set to ON to compile with ISO/TP protocol support" OFF)
option(BUILD_SYSTEMD "Set to ON to compile with systemd additional support" ${BUILD_SYSTEMD_DEFAULT})
option(BUILD_LOAD_TESTS "Set to ON to build load tests" OFF)
option(FAULT_INJECTION "Set to ON to enable fault injection" OFF)
option(INSTALL_LIB "Set to ON to install library and headers" OFF)
option(TESTSUITE_VALGRIND "Set to ON to make tests to run under valgrind (default when CMAKE_BUILD_TYPE=Valgrind)" ${TESTSUITE_VALGRIND_DEFAULT})

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

# find all required libraries
set(BOOST_COMPONENTS log_setup log filesystem program_options)
set(Boost_USE_STATIC_LIBS OFF)
add_definitions(-DBOOST_LOG_DYN_LINK)

if(ANDROID)
    set(BOOST_ROOT ${PREBUILD_BOOST_ROOT})
    set(Boost_INCLUDE_DIR ${BOOST_ROOT}/include)
    set(Boost_LIBRARY_DIR ${BOOST_ROOT}/libs/llvm/${ANDROID_ABI})
    if(Boost_FOUND)
        include_directories(${Boost_INCLUDE_DIRS})
    endif()
endif(ANDROID)

if(BUILD_OPCUA)
    list(APPEND BOOST_COMPONENTS serialization iostreams)
endif(BUILD_OPCUA)

# Mac brew library install paths

if(EXISTS /usr/local/opt/openssl)
  list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/openssl)
endif()

if(EXISTS /usr/local/opt/libarchive)
  list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/libarchive)
endif()

find_package(PkgConfig REQUIRED)
find_package(Boost 1.57.0 COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(LibArchive REQUIRED)
find_package(sodium REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Git)
find_package(Asn1c REQUIRED)
find_package(ZLIB)

if(NOT AKTUALIZR_VERSION)
    if(GIT_EXECUTABLE)
        execute_process(COMMAND sh -c "${GIT_EXECUTABLE} -C ${PROJECT_SOURCE_DIR} describe | tr -d '\n'" OUTPUT_VARIABLE AKTUALIZR_VERSION)
        message(STATUS "Setting version to ${AKTUALIZR_VERSION}")
    else(GIT_EXECUTABLE)
        message(WARNING "Version is not set and git is not available, set version to an arbitrary string")
        set(AKTUALIZR_VERSION "0.0-dev")
    endif(GIT_EXECUTABLE)
endif(NOT AKTUALIZR_VERSION)

add_definitions(-DAKTUALIZR_VERSION="${AKTUALIZR_VERSION}")

if(BUILD_OSTREE)
    find_package(OSTree REQUIRED)
    add_definitions(-DBUILD_OSTREE)
else(BUILD_OSTREE)
    unset(LIBOSTREE_LIBRARIES CACHE)
endif(BUILD_OSTREE)

if(BUILD_DEB)
    pkg_check_modules(LIBDPKG REQUIRED libdpkg)
    if (${LIBDPKG_VERSION} VERSION_GREATER "1.19.3")
        add_definitions(-DLIBDPKG_V011903)
    endif()
    add_definitions(-DBUILD_DEB)
else(BUILD_DEB)
    unset(LIBDPKG_LIBRARIES CACHE)
endif(BUILD_DEB)

if(BUILD_P11)
    find_package(LibP11 REQUIRED)
    add_definitions(-DBUILD_P11)
endif(BUILD_P11)

# Setup PKCS11
if(TEST_PKCS11_MODULE_PATH)
    add_definitions(-DTEST_PKCS11_MODULE_PATH="${TEST_PKCS11_MODULE_PATH}"
                    -DTEST_PKCS11_ENGINE_PATH="${TEST_PKCS11_ENGINE_PATH}")
endif(TEST_PKCS11_MODULE_PATH)

if(BUILD_SOTA_TOOLS)
    find_package(GLIB2 REQUIRED)
    find_program(STRACE NAMES strace)
endif(BUILD_SOTA_TOOLS)

if(BUILD_OPCUA)
    add_definitions(-DOPCUA_SECONDARY_ENABLED)
    if (NOT BUILD_OSTREE)
        message(FATAL_ERROR "BUILD_OPCUA requires BUILD_OSTREE")
    endif(NOT BUILD_OSTREE)
endif(BUILD_OPCUA)

if(BUILD_ISOTP)
    add_definitions(-DISOTP_SECONDARY_ENABLED)
endif(BUILD_ISOTP)

if(BUILD_SYSTEMD)
    find_package(Systemd REQUIRED)
    add_definitions(-DBUILD_SYSTEMD)
else(BUILD_SYSTEMD)
    unset(SYSTEMD_LIBRARY CACHE)
endif(BUILD_SYSTEMD)

if(FAULT_INJECTION)
    find_package(Libfiu REQUIRED)
    add_definitions(-DFIU_ENABLE)
    link_libraries(fiu dl)
    install(PROGRAMS scripts/fiu DESTINATION bin COMPONENT aktualizr)
endif(FAULT_INJECTION)

# set symbols used when compiling
#add_definitions(-DBOOST_LOG_DYN_LINK)

set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")

SET(CMAKE_CXX_FLAGS_VALGRIND "-O1 -g")
SET(CMAKE_C_FLAGS_VALGRIND "-O1 -g")

SET(CMAKE_CXX_FLAGS_ASAN "-O1 -g -fsanitize=address -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer")
SET(CMAKE_C_FLAGS_ASAN "-O1 -g -fsanitize=address -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer")
SET(CMAKE_EXE_LINKER_FLAGS_ASAN "-O1 -g -fsanitize=address -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer")

SET(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer")
SET(CMAKE_C_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer")
SET(CMAKE_EXE_LINKER_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")

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
find_program(CLANG_FORMAT NAMES clang-format-6.0 clang-format)
find_program(CLANG_TIDY NAMES clang-tidy-6.0 clang-tidy)

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
                COMMAND ${PROJECT_SOURCE_DIR}/scripts/check-formatting.sh ${CLANG_FORMAT} ${FILE}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                VERBATIM)
            add_dependencies(check-format ${TARGETNAME_CI})
        endforeach()
    endfunction()
else()
    message(WARNING "clang-format not found, skipping")
    function(aktualizr_clang_format)
    endfunction()
endif()

if(CLANG_TIDY)
    add_custom_target(clang-tidy)
    add_dependencies(qa clang-tidy)
    function(aktualizr_clang_tidy)
        file(RELATIVE_PATH SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(FILE ${ARGN})
            if(${FILE} MATCHES "\\.h")
                # do not run clang-tidy directly on header files, since it
                # ignores them. headers will be checked if they are included by
                # a checked source file
                continue()
            endif()
            string(REPLACE "/" "_" TARGETNAME "aktualizr_clang_tidy-${SUBDIR}-${FILE}")
            add_custom_target(${TARGETNAME}
                COMMAND ${CLANG_TIDY} -quiet -header-filter=\(${CMAKE_SOURCE_DIR}|\\.\\.\)/src/.* --extra-arg-before=-Wno-unknown-warning-option -format-style=file -p ${CMAKE_BINARY_DIR} ${FILE}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                VERBATIM)
            add_dependencies(clang-tidy ${TARGETNAME})
        endforeach()
    endfunction()
else()
    message(WARNING "clang-tidy not found, skipping")
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
include_directories(${PROJECT_SOURCE_DIR}/src/libaktualizr)
include_directories(${PROJECT_SOURCE_DIR}/third_party/googletest/googletest/include)
include_directories(${PROJECT_SOURCE_DIR}/third_party/jsoncpp)
include_directories(${Boost_INCLUDE_DIR})
include_directories(${LIBOSTREE_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIRS})
include_directories(${LIBP11_INCLUDE_DIR})
include_directories(${sodium_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${CURL_INCLUDE_DIR})
include_directories(${LibArchive_INCLUDE_DIR})

set_source_files_properties(third_party/jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS -w)
add_library(jsoncpp OBJECT third_party/jsoncpp/jsoncpp.cpp)

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

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_NAME MATCHES "Android")
        # treat boost headers as system to ignore warnings,
        # this is alternative to in-code diagnostics disabling
        add_definitions(--system-header-prefix=boost/)
    endif ()

    if (WARNING_AS_ERROR)
        add_definitions(-Werror)
    endif ()

    if (PEDANTIC_WARNINGS)
        add_definitions(-Wpedantic)
    endif (PEDANTIC_WARNINGS)
endif()

# General packaging configuration
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "anton.gerasimov@here.com")
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
endif(BUILD_SOTA_TOOLS)

set(CPACK_DEBIAN_AKTUALIZR_PACKAGE_NAME "aktualizr")
set(CPACK_DEBIAN_AKTUALIZR_FILE_NAME "aktualizr.deb")  # only available for CMake >= 3.6.0
set(CPACK_COMPONENT_AKTUALIZR_DESCRIPTION "UPTANE-compliant embedded software update client")
set(CPACK_DEBIAN_AKTUALIZR_PACKAGE_DEPENDS "lshw")
set(CPACK_DEBIAN_AKTUALIZR_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_AKTUALIZR_PACKAGE_CONTROL_EXTRA "${PROJECT_SOURCE_DIR}/config/systemd/debian-control/preinst;${PROJECT_SOURCE_DIR}/config/systemd/debian-control/postinst;${PROJECT_SOURCE_DIR}/config/systemd/debian-control/prerm;")
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
    ${LIBDPKG_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${SYSTEMD_LIBRARY}
    ${ZLIB_LIBRARY})

if(ANDROID)
    list(APPEND AKTUALIZR_EXTERNAL_LIBS liblog.so)
endif()

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set (AKTUALIZR_EXTERNAL_LIBS ${AKTUALIZR_EXTERNAL_LIBS} PARENT_SCOPE)
endif()

set (TEST_LIBS ${AKTUALIZR_EXTERNAL_LIBS} gtest gmock)
if(BUILD_WITH_CODE_COVERAGE)
    set(COVERAGE_LCOV_EXCLUDES '/usr/include/*' ${CMAKE_BINARY_DIR}'*' ${CMAKE_SOURCE_DIR}'/third_party/*' ${CMAKE_SOURCE_DIR}'/tests/*' '*_test.cc')
    include(CodeCoverage)
    set(COVERAGE_COMPILER_FLAGS "--coverage -fprofile-arcs -ftest-coverage" CACHE INTERNAL "")
    list(APPEND TEST_LIBS gcov)
endif(BUILD_WITH_CODE_COVERAGE)

include(CTest)
ENABLE_TESTING()
# It would be great to use GTEST_OUTPUT directly, but I couldn't get it to work.
set(GOOGLE_TEST_OUTPUT --gtest_output=xml:${CMAKE_BINARY_DIR}/results/)

add_subdirectory("fuzz")
add_subdirectory("config")
if (BUILD_LOAD_TESTS)
    # HdrHistogram installs support libraries, which creates junk the aktualizr
    # recipe would have to remove
    # Temporarily remove flag not supported by isotp-c and bitfield.
    remove_definitions(-Wconversion)
    add_subdirectory("third_party/HdrHistogram_c/src")
    add_definitions(-Wconversion)
endif(BUILD_LOAD_TESTS)
add_subdirectory("src")
if(BUILD_PARTIAL)
    add_subdirectory("partial")
endif()
add_subdirectory("tests" EXCLUDE_FROM_ALL)

add_subdirectory("docs")

# Check if some source files were not added sent to `aktualizr_source_file_checks`
#
# Note: does not check `tests` directory which depends too much on conditional
# compilation
file(GLOB_RECURSE ALL_SOURCE_FILES RELATIVE ${CMAKE_SOURCE_DIR}
    src/*.cc src/*.c src/*.h)
foreach(FILE ${ALL_SOURCE_FILES})
    # ignore opcuabridge
    string (FIND ${FILE} "/opcuabridge/" EXCLUDE_DIR_FOUND)
    if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
      continue()
    endif()

    string (FIND ${FILE} "/isotp_conn/" EXCLUDE_DIR_FOUND)
    if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
      continue()
    endif()

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

####### Fuzz tags
option(ENABLE_SANITIZERS "Enables AddressSanitizer and UndefinedBehaviorSanitizer." OFF)

include(CheckCCompilerFlag)
if (ENABLE_SANITIZERS)
    list(APPEND custom_compiler_flags
        -fno-omit-frame-pointer
        -fsanitize=address
        -fsanitize=undefined
        -fsanitize=float-divide-by-zero
        -fsanitize=float-cast-overflow
        -fsanitize-address-use-after-scope
        -fsanitize=integer
        -01
        -fno-sanitize-recover
        )
endif()

# apply custom compiler flags
foreach(compiler_flag ${custom_compiler_flags})
    #remove problematic characters
    string(REGEX REPLACE "[^a-zA-Z0-9]" "" current_variable ${compiler_flag})

    CHECK_C_COMPILER_FLAG(${compiler_flag} "FLAG_SUPPORTED_${current_variable}")
    if (FLAG_SUPPORTED_${current_variable})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${compiler_flag}")
    endif()
endforeach()

# vim: set tabstop=4 shiftwidth=4 expandtab:
