cmake_minimum_required (VERSION 2.6)
cmake_policy(SET CMP0009 NEW)

project(aktualizr)

option(WARNING_AS_ERROR "Treat warnings as errors" ON)
option(PEDANTIC_WARNINGS "Compile with pedantic warnings" OFF)
option(BUILD_WITH_CODE_COVERAGE "Enable gcov code coverage" OFF)
option(BUILD_OSTREE "Set to ON to compile with ostree support" OFF)
option(BUILD_DEB "Set to ON to compile with debian packages support" OFF)
option(BUILD_P11 "Support for key storage in a HSM via PKCS#11" OFF)
option(BUILD_SOTA_TOOLS "Set to ON to build SOTA tools" OFF)
option(BUILD_ISOTP "Set to ON to build ISO-TP" OFF)
option(BUILD_OPCUA "Set to ON to compile with OPC-UA protocol support" OFF)
option(BUILD_SYSTEMD "Set to ON to compile with systemd additional support" ON)
option(INSTALL_LIB "Set to ON to install library and headers" OFF)

set(logger_type "boost" CACHE STRING "")
set(storage_type "sqlite" CACHE STRING "")

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "Aktualizr does not support building in the source tree. Please remove CMakeCache.txt and the CMakeFiles/ directory, then create a subdirectory to build in: mkdir build; cd build; cmake ..")
endif()

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules)
configure_file(CTestCustom.cmake CTestCustom.cmake)

unset(AKTUALIZR_CHECKED_SRCS CACHE)

# find all required libraries
set(Boost_USE_STATIC_LIBS ON)
set(BOOST_COMPONENTS log_setup log filesystem program_options system)

if(BUILD_OPCUA)
    list(APPEND BOOST_COMPONENTS serialization iostreams)
endif(BUILD_OPCUA)

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

if(NOT AKTUALIZR_VERSION)
    if(GIT_EXECUTABLE)
        execute_process(COMMAND sh -c "${GIT_EXECUTABLE} -C ${PROJECT_SOURCE_DIR} describe | tr -d '\n'" OUTPUT_VARIABLE AKTUALIZR_VERSION)
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
    find_package(libdpkg REQUIRED)
    add_definitions(-DBUILD_DEB)
else(BUILD_DEB)
    unset(LIBDPKG_LIBRARIES CACHE)
endif(BUILD_DEB)

if(BUILD_P11)
    find_package(LibP11 REQUIRED)
    add_definitions(-DBUILD_P11)
endif(BUILD_P11)

if(BUILD_SOTA_TOOLS)
    find_package(GLIB2 REQUIRED)
    find_program(STRACE NAMES strace)
endif(BUILD_SOTA_TOOLS)

if(BUILD_OPCUA)
    set(OPEN62541_IGNORED_WARNINGS
        "-Wno-unused-parameter -Wno-float-equal -Wno-logical-op")
    add_definitions(-DOPCUA_SECONDARY_ENABLED)
endif(BUILD_OPCUA)

if(BUILD_SYSTEMD)
    find_package(Systemd REQUIRED)
    add_definitions(-DBUILD_SYSTEMD)
else(BUILD_SYSTEMD)
    unset(SYSTEMD_LIBRARY CACHE)
endif(BUILD_SYSTEMD)

# set symbols used when compiling
#add_definitions(-DBOOST_LOG_DYN_LINK)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No CMAKE_BUILD_TYPE specified, defaulting to Valgrind")
    set(CMAKE_BUILD_TYPE Valgrind)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS_DEBUG "-g")

SET(CMAKE_CXX_FLAGS_VALGRIND "-O1")
SET(CMAKE_C_FLAGS_VALGRIND "-O1")


################ QA RULES
add_custom_target(qa)
add_custom_target(check-format)

add_custom_target(format)
add_dependencies(qa format)

# All binaries are built as prerequisites to this target
add_custom_target(build_tests)

# clang-check and clang-format
find_program(CLANG_FORMAT NAMES clang-format-3.8 clang-format)
find_program(CLANG_CHECK NAMES clang-check-6.0 clang-check-3.8 clang-check)
find_program(CLANG_TIDY NAMES clang-tidy-6.0)

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


if(CLANG_CHECK)
    add_custom_target(clang-check)
    add_dependencies(qa clang-check)
    function(aktualizr_clang_check)
        file(RELATIVE_PATH SUBDIR ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(FILE ${ARGN})
            if(NOT ${FILE} MATCHES "\\.h")
                string(REPLACE "/" "_" TARGETNAME "aktualizr_clang_check-${SUBDIR}-${FILE}")
                add_custom_target(${TARGETNAME}
                    COMMAND ${CLANG_CHECK} -analyze -p ${CMAKE_BINARY_DIR} ${FILE}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    VERBATIM)
                # clang-check needs the binary to be built so it knows the compile flags
                add_dependencies(${TARGETNAME} build_tests)
                add_dependencies(clang-check ${TARGETNAME})
            endif()
        endforeach()
    endfunction()
else()
    message(WARNING "clang-check not found, skipping")
    function(aktualizr_clang_check)
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
                COMMAND ${CLANG_TIDY} -quiet -header-filter=\${CMAKE_SOURCE_DIR}/src/.* --extra-arg-before=-Wno-unknown-warning-option -format-style=file -warnings-as-errors=* -p ${CMAKE_BINARY_DIR} ${FILE}
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
    foreach(FILE ${ARGN})
        file(RELATIVE_PATH FULL_FN ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE})
        set(AKTUALIZR_CHECKED_SRCS ${AKTUALIZR_CHECKED_SRCS} ${FULL_FN} CACHE INTERNAL "")
    endforeach()
    aktualizr_clang_check(${ARGN})
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

# Export compile_commands.json for clang-check
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

############### BUILD RULES
#include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/libaktualizr)
include_directories(${PROJECT_SOURCE_DIR}/third_party/jsoncpp)
include_directories(${PROJECT_SOURCE_DIR}/third_party/picojson)
include_directories(${PROJECT_SOURCE_DIR}/third_party/open62541)
include_directories(${Boost_INCLUDE_DIR})
include_directories(${LIBOSTREE_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIRS})
include_directories(${sodium_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${LibArchive_INCLUDE_DIR})

if (CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-fstack-protector-all)
    # Enable maximum set of warnings. -Wno-sign-compare is required because of
    # problems in gtest. -Wswitch-default and -Wconversion would be nice as
    # well, but they also cause problems in gtest.
    add_definitions(-Wall -Wextra -Wformat-security -Wfloat-equal -Wcast-qual -Wlogical-op -Wno-sign-compare)

    if (CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "4.9" OR MAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.9")
        add_definitions(-Wshadow)
    endif ()

    if(WARNING_AS_ERROR)
        add_definitions(-Werror)
    endif()

    if (PEDANTIC_WARNINGS)
        add_definitions(-Wpedantic -Wswitch-default -Wsign-compare -Wconversion)
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


add_subdirectory("src")
add_subdirectory("fuzz")
add_subdirectory("config")

add_subdirectory("src/sota_tools")

add_subdirectory("src/aktualizr_secondary")

add_subdirectory("src/aktualizr_info")

add_subdirectory("src/aktualizr_check_discovery")

include(CTest)
add_subdirectory("tests" EXCLUDE_FROM_ALL)

add_subdirectory("docs")

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
