# Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8.1)

pkg_check_modules(GLIB glib-2.0)

include_directories(.
       ./src-gen/core
       ./src-gen/dbus
       ${COMMONAPI_INCLUDE_DIRS}
       ${DBus_INCLUDE_DIRS}
       ${gtest_SOURCE_DIR}/include
       ${GLIB_INCLUDE_DIRS}
)

if ("${USE_INSTALLED_DBUS}" STREQUAL "OFF")
    set(DBus_LDFLAGS dbus-1)
    link_directories(
        ${DBus_INCLUDE_DIRS}/dbus/.libs
    )
endif()

set(VERSION "v1")

set(TestInterfaceSources src-gen/core/commonapi/tests/DerivedTypeCollection.cpp
                         src-gen/core/${VERSION}/commonapi/tests/TestInterfaceStubDefault.cpp)

set(TestInterfaceDBusSources ${TestInterfaceSources}
                             src-gen/dbus/${VERSION}/commonapi/tests/TestInterfaceDBusProxy.cpp
                             src-gen/dbus/${VERSION}/commonapi/tests/TestInterfaceDBusStubAdapter.cpp
                             src-gen/dbus/${VERSION}/fake/legacy/service/LegacyInterfaceDBusProxy.cpp
	                         src-gen/dbus/${VERSION}/fake/legacy/service/LegacyInterfaceNoObjectManagerDBusProxy.cpp)

set(TestInterfaceManagerSources src-gen/core/${VERSION}/commonapi/tests/TestInterfaceManagerStubDefault.cpp)

set(TestInterfaceManagerDBusSources ${TestInterfaceManagerSources}
                                    src-gen/dbus/${VERSION}/commonapi/tests/TestInterfaceManagerDBusProxy.cpp
                                    src-gen/dbus/${VERSION}/commonapi/tests/TestInterfaceManagerDBusStubAdapter.cpp)

set(FreedesktopPropertiesSources src-gen/core/${VERSION}/commonapi/tests/TestFreedesktopInterfaceStubDefault.cpp
                                 src-gen/core/${VERSION}/commonapi/tests/TestFreedesktopDerivedInterfaceStubDefault.cpp)

set(FreedesktopPropertiesDBusSources ${FreedesktopPropertiesSources}
                                     src-gen/dbus/${VERSION}/commonapi/tests/TestFreedesktopInterfaceDBusProxy.cpp
                                     src-gen/dbus/${VERSION}/commonapi/tests/TestFreedesktopInterfaceDBusStubAdapter.cpp
                                     src-gen/dbus/${VERSION}/commonapi/tests/TestFreedesktopDerivedInterfaceDBusProxy.cpp
                                     src-gen/dbus/${VERSION}/commonapi/tests/TestFreedesktopDerivedInterfaceDBusStubAdapter.cpp)

set(ManagedSources src-gen/core/${VERSION}/commonapi/tests/managed/LeafInterfaceStubDefault.cpp
                   src-gen/core/${VERSION}/commonapi/tests/managed/BranchInterfaceStubDefault.cpp
                   src-gen/core/${VERSION}/commonapi/tests/managed/RootInterfaceStubDefault.cpp
                   src-gen/core/${VERSION}/commonapi/tests/managed/SecondRootStubDefault.cpp)

set(ManagedDBusSources ${ManagedSources}
                       src-gen/dbus/${VERSION}/commonapi/tests/managed/RootInterfaceDBusProxy.cpp
                       src-gen/dbus/${VERSION}/commonapi/tests/managed/RootInterfaceDBusStubAdapter.cpp
                       src-gen/dbus/${VERSION}/commonapi/tests/managed/LeafInterfaceDBusProxy.cpp
                       src-gen/dbus/${VERSION}/commonapi/tests/managed/LeafInterfaceDBusStubAdapter.cpp
                       src-gen/dbus/${VERSION}/commonapi/tests/managed/BranchInterfaceDBusProxy.cpp
                       src-gen/dbus/${VERSION}/commonapi/tests/managed/BranchInterfaceDBusStubAdapter.cpp
                       src-gen/dbus/${VERSION}/commonapi/tests/managed/SecondRootDBusProxy.cpp
                       src-gen/dbus/${VERSION}/commonapi/tests/managed/SecondRootDBusStubAdapter.cpp)

set(ExtendedInterfaceSources src-gen/core/${VERSION}/commonapi/tests/ExtendedInterfaceStubDefault.cpp)

set(ExtendedInterfaceDBusSources ${ExtendedInterfaceSources}
                                 src-gen/dbus/${VERSION}/commonapi/tests/ExtendedInterfaceDBusProxy.cpp
                                 src-gen/dbus/${VERSION}/commonapi/tests/ExtendedInterfaceDBusStubAdapter.cpp)


set(TEST_LINK_LIBRARIES -Wl,--no-as-needed CommonAPI-DBus -Wl,--as-needed CommonAPI ${DBus_LDFLAGS} ${DL_LIBRARY} gtest ${PTHREAD_LIBRARY})

set(TEST_LINK_LIBRARIES_WITHOUT_COMMONAPI_DBUS CommonAPI gtest ${PTHREAD_LIBRARY})

##############################################################################
# DBusConnectionTest
##############################################################################

add_executable(DBusConnectionTest DBusConnectionTest.cpp)

target_link_libraries(DBusConnectionTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusVariantOutputStreamTest
##############################################################################

add_executable(DBusVariantOutputStreamTest DBusVariantOutputStreamTest.cpp)

target_link_libraries(DBusVariantOutputStreamTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusDaemonProxyTest
##############################################################################


add_executable(DBusDaemonProxyTest DBusDaemonProxyTest.cpp)

target_link_libraries(DBusDaemonProxyTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusVariantTest
##############################################################################

add_executable(DBusVariantTest DBusVariantTest.cpp)

target_link_libraries(DBusVariantTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusClientIdTest
##############################################################################

add_executable(DBusClientIdTest DBusClientIdTest.cpp)

target_link_libraries(DBusClientIdTest ${TEST_LINK_LIBRARIES})

##############################################################################
# Add for every test a dependency to gtest
##############################################################################

add_dependencies(DBusConnectionTest gtest)
add_dependencies(DBusVariantOutputStreamTest gtest)
add_dependencies(DBusDaemonProxyTest gtest)
add_dependencies(DBusVariantTest gtest)
add_dependencies(DBusClientIdTest gtest)

##############################################################################
# Add tests to the target build_tests
##############################################################################

add_dependencies(build_tests DBusConnectionTest)
add_dependencies(build_tests DBusVariantOutputStreamTest)
add_dependencies(build_tests DBusDaemonProxyTest)
add_dependencies(build_tests DBusVariantTest)
add_dependencies(build_tests DBusClientIdTest)

##############################################################################
# Add tests
##############################################################################

add_test(NAME DBusConnectionTest COMMAND DBusConnectionTest)
add_test(NAME DBusVariantOutputStreamTest COMMAND DBusVariantOutputStreamTest)
add_test(NAME DBusDaemonProxyTest COMMAND DBusDaemonProxyTest)
add_test(NAME DBusVariantTest COMMAND DBusVariantTest)
add_test(NAME DBusClientIdTest COMMAND DBusClientIdTest)

