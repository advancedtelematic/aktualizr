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
# DBusManagedTest
##############################################################################

add_executable(DBusManagedTest DBusManagedTest.cpp
                               ${ManagedDBusSources})
target_link_libraries(DBusManagedTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusMainLoopIntegrationTest
##############################################################################

add_executable(DBusMainLoopIntegrationTest DBusMainLoopIntegrationTest.cpp
                                           ${TestInterfaceDBusSources})
if (MSVC)
target_link_libraries(DBusMainLoopIntegrationTest ${TEST_LINK_LIBRARIES} ws2_32)
else()
target_link_libraries(DBusMainLoopIntegrationTest ${TEST_LINK_LIBRARIES} ${GLIB_LDFLAGS})
endif()

##############################################################################
# DBusAddressTranslatorTest
##############################################################################


add_executable(DBusAddressTranslatorTest DBusAddressTranslatorTest.cpp
                                         ${TestInterfaceDBusSources})

target_link_libraries(DBusAddressTranslatorTest ${TEST_LINK_LIBRARIES})

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
# DBusCommunicationTest
##############################################################################

add_executable(DBusCommunicationTest DBusCommunicationTest.cpp
                                     ${TestInterfaceDBusSources})

target_link_libraries(DBusCommunicationTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusDaemonProxyTest
##############################################################################


add_executable(DBusDaemonProxyTest DBusDaemonProxyTest.cpp)

target_link_libraries(DBusDaemonProxyTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusInputStreamTest
##############################################################################

add_executable(DBusInputStreamTest DBusInputStreamTest.cpp
                                   ${TestInterfaceDBusSources})

target_link_libraries(DBusInputStreamTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusOutputStreamTest
##############################################################################


add_executable(DBusOutputStreamTest DBusOutputStreamTest.cpp
                                    ${TestInterfaceDBusSources})

target_link_libraries(DBusOutputStreamTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusFactoryTest
##############################################################################

add_executable(DBusFactoryTest DBusFactoryTest.cpp
                               ${TestInterfaceDBusSources})

target_link_libraries(DBusFactoryTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusMultipleConnectionTest
##############################################################################

add_executable(DBusMultipleConnectionTest DBusMultipleConnectionTest.cpp
                                          ${TestInterfaceDBusSources})

target_link_libraries(DBusMultipleConnectionTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusProxyTest
##############################################################################

add_executable(DBusProxyTest DBusProxyTest.cpp
                             ${TestInterfaceDBusSources}
                             ${ExtendedInterfaceDBusSources})

target_link_libraries(DBusProxyTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusFreedesktopPropertiesTest
##############################################################################

add_executable(DBusFreedesktopPropertiesTest DBusFreedesktopPropertiesTest.cpp
                                             ${TestInterfaceSources}
                                             ${FreedesktopPropertiesDBusSources})

target_link_libraries(DBusFreedesktopPropertiesTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusVariantTest
##############################################################################

add_executable(DBusVariantTest DBusVariantTest.cpp)

target_link_libraries(DBusVariantTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusRuntimeTest
##############################################################################

add_executable(DBusRuntimeTest DBusRuntimeTest.cpp)

target_link_libraries(DBusRuntimeTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusClientIdTest
##############################################################################

add_executable(DBusClientIdTest DBusClientIdTest.cpp)

target_link_libraries(DBusClientIdTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusBroadcastTest
##############################################################################

add_executable(DBusBroadcastTest ${TestInterfaceDBusSources}
                                 ${TestInterfaceManagerDBusSources}
                                 DBusBroadcastTest.cpp)

target_link_libraries(DBusBroadcastTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusPolymorphicTest
##############################################################################


add_executable(DBusPolymorphicTest ${TestInterfaceDBusSources}
                                   DBusPolymorphicTest.cpp)

target_link_libraries(DBusPolymorphicTest ${TEST_LINK_LIBRARIES})

##############################################################################
# DBusLoadTest
##############################################################################

add_executable(DBusLoadTest DBusLoadTest.cpp
                            ${TestInterfaceDBusSources})

target_link_libraries(DBusLoadTest ${TEST_LINK_LIBRARIES})

##############################################################################
# Add for every test a dependency to gtest
##############################################################################

add_dependencies(DBusManagedTest gtest)
add_dependencies(DBusMainLoopIntegrationTest gtest)
add_dependencies(DBusAddressTranslatorTest gtest)
add_dependencies(DBusConnectionTest gtest)
add_dependencies(DBusVariantOutputStreamTest gtest)
add_dependencies(DBusCommunicationTest gtest)
add_dependencies(DBusDaemonProxyTest gtest)
add_dependencies(DBusInputStreamTest gtest)
add_dependencies(DBusOutputStreamTest gtest)
add_dependencies(DBusFactoryTest gtest)
add_dependencies(DBusMultipleConnectionTest gtest)
add_dependencies(DBusProxyTest gtest)
add_dependencies(DBusFreedesktopPropertiesTest gtest)
add_dependencies(DBusVariantTest gtest)
add_dependencies(DBusRuntimeTest gtest)                
add_dependencies(DBusClientIdTest gtest)
add_dependencies(DBusBroadcastTest gtest)
add_dependencies(DBusPolymorphicTest gtest)
add_dependencies(DBusLoadTest gtest)

##############################################################################
# Add tests to the target build_tests
##############################################################################

add_dependencies(build_tests DBusManagedTest)
add_dependencies(build_tests DBusMainLoopIntegrationTest)
add_dependencies(build_tests DBusAddressTranslatorTest)
add_dependencies(build_tests DBusConnectionTest)
add_dependencies(build_tests DBusVariantOutputStreamTest)
add_dependencies(build_tests DBusCommunicationTest)
add_dependencies(build_tests DBusDaemonProxyTest)
add_dependencies(build_tests DBusInputStreamTest)
add_dependencies(build_tests DBusOutputStreamTest)
add_dependencies(build_tests DBusFactoryTest)
add_dependencies(build_tests DBusMultipleConnectionTest)
add_dependencies(build_tests DBusProxyTest)
add_dependencies(build_tests DBusFreedesktopPropertiesTest)
add_dependencies(build_tests DBusVariantTest)
add_dependencies(build_tests DBusRuntimeTest)              
add_dependencies(build_tests DBusClientIdTest)
add_dependencies(build_tests DBusBroadcastTest)
add_dependencies(build_tests DBusPolymorphicTest)
add_dependencies(build_tests DBusLoadTest)

##############################################################################
# Add tests
##############################################################################

add_test(NAME DBusManagedTest COMMAND DBusManagedTest)
add_test(NAME DBusMainLoopIntegrationTest COMMAND DBusMainLoopIntegrationTest)
add_test(NAME DBusAddressTranslatorTest WORKING_DIRECTORY ${libcommonapi-dbus_SOURCE_DIR} COMMAND DBusAddressTranslatorTest)
add_test(NAME DBusConnectionTest COMMAND DBusConnectionTest)
add_test(NAME DBusVariantOutputStreamTest COMMAND DBusVariantOutputStreamTest)
add_test(NAME DBusCommunicationTest COMMAND DBusCommunicationTest)
add_test(NAME DBusDaemonProxyTest COMMAND DBusDaemonProxyTest)
add_test(NAME DBusInputStreamTest COMMAND DBusInputStreamTest)
add_test(NAME DBusOutputStreamTest COMMAND DBusOutputStreamTest)
add_test(NAME DBusFactoryTest COMMAND DBusFactoryTest)
add_test(NAME DBusMultipleConnectionTest COMMAND DBusMultipleConnectionTest)
add_test(NAME DBusProxyTest COMMAND DBusProxyTest)
add_test(NAME DBusFreedesktopPropertiesTest COMMAND DBusFreedesktopPropertiesTest)
add_test(NAME DBusVariantTest COMMAND DBusVariantTest)
add_test(NAME DBusRuntimeTest COMMAND DBusRuntimeTest)
add_test(NAME DBusClientIdTest COMMAND DBusClientIdTest)
add_test(NAME DBusBroadcastTest COMMAND DBusBroadcastTest)
add_test(NAME DBusPolymorphicTest COMMAND DBusPolymorphicTest)
add_test(NAME DBusLoadTest COMMAND DBusLoadTest)

#DBusManagedTest_SOURCES = \
#        src/test/DBusManagedTest.cpp \
#        ${ManagedDBusSources}
#DBusManagedTest_CPPFLAGS = ${AM_CPPFLAGS} ${GTEST_CPPFLAGS} ${GLIB_CFLAGS}
#DBusManagedTest_CXXFLAGS = ${GTEST_CXXFLAGS} ${GLIB_CFLAGS}
#DBusManagedTest_LDADD = ${LDADD_FOR_GTEST} ${GLIB_LIBS}

#AM_CPPFLAGS = \
#        -include $(top_builddir)/build-aux/config.h \
#        -I$(top_srcdir)/src \
#        -I$(top_srcdir)/src/test \
#        ${COMMONAPI_CFLAGS} \
#        ${DBUS_CFLAGS} \
#        -DCOMMONAPI_INTERNAL_COMPILATION \
#        -DCOMMONAPI_DBUS_VERSION_MAJOR=${LIBCOMMONAPI_DBUS_CURRENT} \
#        -DCOMMONAPI_DBUS_VERSION_MINOR=${LIBCOMMONAPI_DBUS_REVISION}

#COMMONAPI_CFLAGS = -I/usr/local/include/CommonAPI-2.1  
#COMMONAPI_LIBS = -L/usr/local/lib -lCommonAPI  
#DBUS_CFLAGS = -I/usr/local/include/dbus-1.0 -I/usr/local/lib/dbus-1.0/include  
#DBUS_LIBS = -L/usr/local/lib -ldbus-1 -lpthread -lrt  

#GTEST_CPPFLAGS:    -I/home/genius/googletest/gtest-1.7.0/include -I/home/genius/googletest/gtest-1.7.0/include

#GLIB_CFLAGS='-I/usr/include/glib-2.0 -I/usr/lib/i386-linux-gnu/glib-2.0/include  '



#GTEST_CXXFLAGS:    -pthread

#LDADD_FOR_GTEST_WITHOUT_LIBCOMMON_API_DBUS = ${GTEST_LIBS} ${LDADD}
#LDADD_FOR_GTEST = libCommonAPI-DBus.la ${LDADD_FOR_GTEST_WITHOUT_LIBCOMMON_API_DBUS}
#GTEST_LIBS:        /home/genius/googletest/gtest-1.7.0/lib/libgtest.la -pthread


#GLIB_LIBS='-lglib-2.0  '

