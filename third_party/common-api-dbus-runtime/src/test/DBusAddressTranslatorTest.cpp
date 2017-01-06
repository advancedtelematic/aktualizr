// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.


#include <gtest/gtest.h>
#include <fstream>
#include <thread>

#include <CommonAPI/CommonAPI.hpp>

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#define VERSION v1_0

#include <CommonAPI/Types.hpp>
#include <CommonAPI/Utils.hpp>
#include <CommonAPI/DBus/DBusAddressTranslator.hpp>
#include <CommonAPI/DBus/DBusUtils.hpp>
#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>

#include "commonapi/tests/PredefinedTypeCollection.hpp"
#include "commonapi/tests/DerivedTypeCollection.hpp"
#include "v1/commonapi/tests/TestInterfaceProxy.hpp"
#include "v1/commonapi/tests/TestInterfaceStubDefault.hpp"
#include "v1/commonapi/tests/TestInterfaceDBusStubAdapter.hpp"

#include <v1/fake/legacy/service/LegacyInterfaceProxy.hpp>
#include <v1/fake/legacy/service/LegacyInterfaceNoObjectManagerProxy.hpp>

static const std::string domain = "local";

static const std::string fileString =
""
"[not#a$valid/address]\n"
"[]\n"
"   98t3hpgjvqpvnü0 t4b+qßk4 kv+üg4krgv+ß4krgv+ßkr\n"
"[too.short:address]\n"
"[incomplete:address:]\n"
"[:address:incomplete]\n"
"[]đwqervqerverver\n"
"[too:long:address:here]\n"
"jfgv2nqp3 riqpnvi39r[]"
"\n"
"[local:no.nothing.service:no.nothing.instance]\n"
"\n"
"[local:service:instance]\n"
"service=connection.name\n"
"path=/path/to/object\n"
"interface=service.name\n"
"\n"
"[local:no.interface.service:no.interface.instance]\n"
"service=no.interface.connection\n"
"path=/no/interface/path\n"
"\n"
"[local:no.connection.service:no.connection.instance]\n"
"path=/no/connection/path\n"
"interface=no.connection.interface\n"
"\n"
"[local:no.object.service:no.object.instance]\n"
"service=no.object.connection\n"
"interface=no.object.interface\n"
"\n"
"[local:only.interface.service:only.interface.instance]\n"
"interface=only.interface.interface\n"
"\n"
"[local:only.connection.service:only.connection.instance]\n"
"service=only.connection.connection\n"
"\n"
"[local:only.object.service:only.object.instance]\n"
"path=/only/object/path\n"
"\n"
"[local:fake.legacy.service.LegacyInterface:fake.legacy.service]\n"
"service=fake.legacy.service.connection\n"
"path=/some/legacy/path/6259504\n"
"interface=fake.legacy.service.LegacyInterface\n"
// all tests run within the same binary under windows, meaning the config file will only be read once. That's why we already have to add the factory configuration used by DBusFactoryTest and the predifined instances for DBusServiceRegistryTest here.
#ifdef WIN32
"[local:Interface1:predefined.Instance1]\n"
"service=DBusServiceRegistryTest.Predefined.Service\n"
"path=/tests/predefined/Object1\n"
"interface=tests.Interface1\n"
"[local:Interface1:predefined.Instance2]\n"
"service=DBusServiceRegistryTest.Predefined.Service\n"
"path=/tests/predefined/Object2\n"
"interface=tests.Interface1\n"
"[local:Interface2:predefined.Instance]\n"
"service=DBusServiceRegistryTest.Predefined.Service\n"
"path=/tests/predefined/Object1\n"
"interface=tests.Interface2\n"
"[factory$session]\n"
"dbus_bustype=session\n"
"[factory$system]\n"
"dbus_bustype=system\n"
#endif
;

static const std::vector<std::string> commonApiAddresses = {
    "local:no.nothing.service:no.nothing.instance",
    "local:service:instance",
    "local:no.interface.service:no.interface.instance",
    "local:no.connection.service:no.connection.instance",
    "local:no.object.service:no.object.instance",
    "local:only.interface.service:only.interface.instance",
    "local:only.connection.service:only.connection.instance",
    "local:only.object.service:only.object.instance",
    "local:fake.legacy.service.LegacyInterface:fake.legacy.service",
    "local:fake.legacy.service.LegacyInterfaceNoObjectManager:fake.legacy.service"
};

typedef std::vector<CommonAPI::DBus::DBusAddress>::value_type vt;
static const std::vector<CommonAPI::DBus::DBusAddress> dbusAddresses = {
                vt("no.nothing.service_no.nothing.instance", "/no/nothing/instance", "no.nothing.service"),
                vt("service.name_connection.name", "/path/to/object", "service.name"),
                vt("no.interface.service_no.interface.instance", "/no/interface/instance", "no.interface.service"),
                vt("no.connection.service_no.connection.instance", "/no/connection/instance", "no.connection.service"),
                vt("no.object.service_no.object.instance", "/no/object/instance", "no.object.service"),
                vt("only.interface.service_only.interface.instance", "/only/interface/instance", "only.interface.service"),
                vt("only.connection.service_only.connection.instance", "/only/connection/instance", "only.connection.service"),
                vt("only.object.service_only.object.instance", "/only/object/instance", "only.object.service"),
                vt("fake.legacy.service.connection", "/some/legacy/path/6259504", "fake.legacy.service.LegacyInterface"),
                vt("fake.legacy.service.connection", "/some/legacy/path/6259504", "fake.legacy.service.LegacyInterfaceNoObjectManager")
};

class AddressTranslatorTest: public ::testing::Test {

protected:
    void SetUp() {
    }

    virtual void TearDown() {
        usleep(30000);
    }
    std::string configFileName_;
};

TEST_F(AddressTranslatorTest, InstanceCanBeRetrieved) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> dbusAddressTranslator = CommonAPI::DBus::DBusAddressTranslator::get();
    ASSERT_TRUE((bool) dbusAddressTranslator);
}

TEST_F(AddressTranslatorTest, ParsesDBusAddresses) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    for(unsigned int i = 0; i < commonApiAddresses.size(); i++) {
        std::string interfaceName, connectionName, objectPath;
        CommonAPI::DBus::DBusAddress dbusAddress;
        translator->translate(commonApiAddresses[i], dbusAddress);
        std::cout << dbusAddress.getService() << " " << dbusAddress.getObjectPath() << " " << dbusAddress.getInterface() << std::endl;
        ASSERT_EQ(dbusAddresses[i].getService(), dbusAddress.getService());
        ASSERT_EQ(dbusAddresses[i].getObjectPath(), dbusAddress.getObjectPath());
        ASSERT_EQ(dbusAddresses[i].getInterface(), dbusAddress.getInterface());
    }
}

TEST_F(AddressTranslatorTest, ParsesCommonAPIAddresses) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    for(unsigned int i = 0; i < commonApiAddresses.size(); i++) {
        CommonAPI::Address commonApiAddress;
        translator->translate(CommonAPI::DBus::DBusAddress(dbusAddresses[i].getService(), dbusAddresses[i].getObjectPath(), dbusAddresses[i].getInterface()), commonApiAddress);
        std::cout << dbusAddresses[i].getService() << " " << dbusAddresses[i].getObjectPath() << " " << dbusAddresses[i].getInterface() << std::endl;
        std::cout << commonApiAddress.getDomain() << " " << commonApiAddress.getInterface() << " " << commonApiAddress.getInstance() << std::endl;
        ASSERT_EQ(commonApiAddresses[i], commonApiAddress.getAddress());
    }
}

TEST_F(AddressTranslatorTest, InsertAddressPossible) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    std::string commonApiAddressRef = "local:my.service:my.instance";

    CommonAPI::DBus::DBusAddress dbusAddressInsertRef("my.new.service_my.new.instance", "/my/new/instance", "my.new.service");
    CommonAPI::DBus::DBusAddress dbusAddressSecondInsertRef("my.new.second.service_my.new.second.instance", "/my/new/second/instance", "my.new.second.service");
    std::string commonApiSecondInsertAddressRef = "local:my.new.second.service:my.new.second.instance";

    CommonAPI::DBus::DBusAddress dbusAddressResult;
    CommonAPI::Address commonApiAddressResult;

    // insert new address
    translator->insert(commonApiAddressRef,
            dbusAddressInsertRef.getService(),
            dbusAddressInsertRef.getObjectPath(),
            dbusAddressInsertRef.getInterface());

    //check inserted address
    translator->translate(commonApiAddressRef, dbusAddressResult);
    std::cout << dbusAddressResult.getService() << " " << dbusAddressResult.getObjectPath() << " " << dbusAddressResult.getInterface() << std::endl;
    ASSERT_EQ(dbusAddressInsertRef.getService(), dbusAddressResult.getService());
    ASSERT_EQ(dbusAddressInsertRef.getObjectPath(), dbusAddressResult.getObjectPath());
    ASSERT_EQ(dbusAddressInsertRef.getInterface(), dbusAddressResult.getInterface());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressInsertRef.getService(), dbusAddressInsertRef.getObjectPath(), dbusAddressInsertRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressInsertRef.getService() << " " << dbusAddressInsertRef.getObjectPath() << " " << dbusAddressInsertRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressRef, commonApiAddressResult.getAddress());

    // try overwriting address
    translator->insert(commonApiAddressRef,
            dbusAddressSecondInsertRef.getService(),
            dbusAddressSecondInsertRef.getObjectPath(),
            dbusAddressSecondInsertRef.getInterface());

    //check overwritten not possible
    translator->translate(commonApiAddressRef, dbusAddressResult);
    std::cout << dbusAddressResult.getService() << " " << dbusAddressResult.getObjectPath() << " " << dbusAddressResult.getInterface() << std::endl;
    ASSERT_EQ(dbusAddressInsertRef.getService(), dbusAddressResult.getService());
    ASSERT_EQ(dbusAddressInsertRef.getObjectPath(), dbusAddressResult.getObjectPath());
    ASSERT_EQ(dbusAddressInsertRef.getInterface(), dbusAddressResult.getInterface());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressSecondInsertRef.getService(), dbusAddressSecondInsertRef.getObjectPath(), dbusAddressSecondInsertRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressSecondInsertRef.getService() << " " << dbusAddressSecondInsertRef.getObjectPath() << " " << dbusAddressSecondInsertRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiSecondInsertAddressRef, commonApiAddressResult.getAddress());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressInsertRef.getService(), dbusAddressInsertRef.getObjectPath(), dbusAddressInsertRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressInsertRef.getService() << " " << dbusAddressInsertRef.getObjectPath() << " " << dbusAddressInsertRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressRef, commonApiAddressResult.getAddress());
}

TEST_F(AddressTranslatorTest, InsertUniqueBusNameTranslate) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    translator->insert("local:my.Interface:busname.legacy.service_1_133",
                        ":1.133",        /* unique bus name */
                        "/org/busname/legacy/service",
                        "busname.legacy.service");

    CommonAPI::DBus::DBusAddress dbusAddress;

    translator->translate("local:my.Interface:busname.legacy.service_1_133", dbusAddress);

    ASSERT_EQ(":1.133", dbusAddress.getService());
    ASSERT_EQ("busname.legacy.service", dbusAddress.getInterface());
    ASSERT_EQ("/org/busname/legacy/service", dbusAddress.getObjectPath());
}

TEST_F(AddressTranslatorTest, InsertAddressNotPossibleConflictTranslate) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    CommonAPI::DBus::DBusAddress dbusAddressRef("my.service.translate_my.instance.translate", "/my/instance/translate", "my.service.translate");
    std::string commonApiAddressRef = "local:my.service.translate:my.instance.translate";

    CommonAPI::DBus::DBusAddress dbusAddressInsertRef("my.new.service.translate_my.new.instance.translate", "/my/new/instance/translate", "my.new.service.translate");
    std::string commonApiAddressInsertRef = "local:my.new.service.translate:my.new.instance.translate";

    CommonAPI::DBus::DBusAddress dbusAddressResult;
    CommonAPI::Address commonApiAddressResult;

    // insertion via translate
    translator->translate(commonApiAddressRef, dbusAddressResult);
    std::cout << dbusAddressResult.getService() << " " << dbusAddressResult.getObjectPath() << " " << dbusAddressResult.getInterface() << std::endl;
    ASSERT_EQ(dbusAddressRef.getService(), dbusAddressResult.getService());
    ASSERT_EQ(dbusAddressRef.getObjectPath(), dbusAddressResult.getObjectPath());
    ASSERT_EQ(dbusAddressRef.getInterface(), dbusAddressResult.getInterface());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressRef.getService(), dbusAddressRef.getObjectPath(), dbusAddressRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressRef.getService() << " " << dbusAddressRef.getObjectPath() << " " << dbusAddressRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressRef, commonApiAddressResult.getAddress());

    // try to overwrite address
    translator->insert(commonApiAddressRef,
            dbusAddressInsertRef.getService(),
            dbusAddressInsertRef.getObjectPath(),
            dbusAddressInsertRef.getInterface());

    //check that inserting was not possible
    translator->translate(commonApiAddressRef, dbusAddressResult);
    std::cout << dbusAddressResult.getService() << " " << dbusAddressResult.getObjectPath() << " " << dbusAddressResult.getInterface() << std::endl;
    ASSERT_EQ(dbusAddressRef.getService(), dbusAddressResult.getService());
    ASSERT_EQ(dbusAddressRef.getObjectPath(), dbusAddressResult.getObjectPath());
    ASSERT_EQ(dbusAddressRef.getInterface(), dbusAddressResult.getInterface());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressInsertRef.getService(), dbusAddressInsertRef.getObjectPath(), dbusAddressInsertRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressInsertRef.getService() << " " << dbusAddressInsertRef.getObjectPath() << " " << dbusAddressInsertRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressInsertRef, commonApiAddressResult.getAddress());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressRef.getService(), dbusAddressRef.getObjectPath(), dbusAddressRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressRef.getService() << " " << dbusAddressRef.getObjectPath() << " " << dbusAddressRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressRef, commonApiAddressResult.getAddress());

}

TEST_F(AddressTranslatorTest, InsertAddressNotPossibleConflictConfigFile) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    CommonAPI::DBus::DBusAddress dbusAddressInsertRef("my.new.service.config_my.new.instance.config", "/my/new/instance/config", "my.new.service.config");
    std::string commonApiAddressInsertRef = "local:my.new.service.config:my.new.instance.config";

    CommonAPI::DBus::DBusAddress dbusAddressResult;
    CommonAPI::Address commonApiAddressResult;

    // try to overwrite address
    translator->insert(commonApiAddresses[1],
            dbusAddressInsertRef.getService(),
            dbusAddressInsertRef.getObjectPath(),
            dbusAddressInsertRef.getInterface());

    //check that inserting was not possible
    translator->translate(commonApiAddresses[1], dbusAddressResult);
    std::cout << dbusAddressResult.getService() << " " << dbusAddressResult.getObjectPath() << " " << dbusAddressResult.getInterface() << std::endl;
    ASSERT_EQ(dbusAddresses[1].getService(), dbusAddressResult.getService());
    ASSERT_EQ(dbusAddresses[1].getObjectPath(), dbusAddressResult.getObjectPath());
    ASSERT_EQ(dbusAddresses[1].getInterface(), dbusAddressResult.getInterface());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressInsertRef.getService(), dbusAddressInsertRef.getObjectPath(), dbusAddressInsertRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressInsertRef.getService() << " " << dbusAddressInsertRef.getObjectPath() << " " << dbusAddressInsertRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressInsertRef, commonApiAddressResult.getAddress());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddresses[1].getService(), dbusAddresses[1].getObjectPath(), dbusAddresses[1].getInterface()), commonApiAddressResult);
    std::cout << dbusAddresses[1].getService() << " " << dbusAddresses[1].getObjectPath() << " " << dbusAddresses[1].getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddresses[1], commonApiAddressResult.getAddress());
}

TEST_F(AddressTranslatorTest, UniqueAddressHandlingTranslateWorks) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    std::string commonApiAddressRef = "local:my.unique.translate.interface:my.unique.translate.instance";
    CommonAPI::DBus::DBusAddress dbusAddressInsertRef(":1.6", "/my/unique/translate/instance", "my.unique.translate.interface");

    CommonAPI::DBus::DBusAddress dbusAddressResult;
    CommonAPI::Address commonApiAddressResult;

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressInsertRef.getService(), dbusAddressInsertRef.getObjectPath(), dbusAddressInsertRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressInsertRef.getService() << " " << dbusAddressInsertRef.getObjectPath() << " " << dbusAddressInsertRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressRef, commonApiAddressResult.getAddress());

    translator->translate(commonApiAddressRef, dbusAddressResult);
    std::cout << dbusAddressResult.getService() << " " << dbusAddressResult.getObjectPath() << " " << dbusAddressResult.getInterface() << std::endl;
    ASSERT_EQ(dbusAddressInsertRef.getService(), dbusAddressResult.getService());
    ASSERT_EQ(dbusAddressInsertRef.getObjectPath(), dbusAddressResult.getObjectPath());
    ASSERT_EQ(dbusAddressInsertRef.getInterface(), dbusAddressResult.getInterface());
}

TEST_F(AddressTranslatorTest, UniqueAddressHandlingInsertWorks) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    std::string commonApiAddressRef = "local:my.unique.insert.other.interface:my.unique.insert.other.instance";
    CommonAPI::DBus::DBusAddress dbusAddressInsertRef(":1.6", "/my/unique/insert/instance", "my.unique.insert.interface");

    CommonAPI::DBus::DBusAddress dbusAddressResult;
    CommonAPI::Address commonApiAddressResult;

    translator->insert(commonApiAddressRef,
                dbusAddressInsertRef.getService(),
                dbusAddressInsertRef.getObjectPath(),
                dbusAddressInsertRef.getInterface());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressInsertRef.getService(), dbusAddressInsertRef.getObjectPath(), dbusAddressInsertRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressInsertRef.getService() << " " << dbusAddressInsertRef.getObjectPath() << " " << dbusAddressInsertRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressRef, commonApiAddressResult.getAddress());

    translator->translate(commonApiAddressRef, dbusAddressResult);
    std::cout << dbusAddressResult.getService() << " " << dbusAddressResult.getObjectPath() << " " << dbusAddressResult.getInterface() << std::endl;
    ASSERT_EQ(dbusAddressInsertRef.getService(), dbusAddressResult.getService());
    ASSERT_EQ(dbusAddressInsertRef.getObjectPath(), dbusAddressResult.getObjectPath());
    ASSERT_EQ(dbusAddressInsertRef.getInterface(), dbusAddressResult.getInterface());
}

TEST_F(AddressTranslatorTest, CheckWellKnownNameTranslateWorks) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    std::string commonApiAddressRef = "local:my.well.translate.interface:my.well.translate.instance";
    CommonAPI::DBus::DBusAddress dbusAddressInsertRef("my.well.known.name", "/my/well/translate/instance", "my.well.translate.interface");

    CommonAPI::DBus::DBusAddress dbusAddressResult;
    CommonAPI::Address commonApiAddressResult;

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressInsertRef.getService(), dbusAddressInsertRef.getObjectPath(), dbusAddressInsertRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressInsertRef.getService() << " " << dbusAddressInsertRef.getObjectPath() << " " << dbusAddressInsertRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressRef, commonApiAddressResult.getAddress());

    translator->translate(commonApiAddressRef, dbusAddressResult);
    std::cout << dbusAddressResult.getService() << " " << dbusAddressResult.getObjectPath() << " " << dbusAddressResult.getInterface() << std::endl;
    ASSERT_EQ(dbusAddressInsertRef.getService(), dbusAddressResult.getService());
    ASSERT_EQ(dbusAddressInsertRef.getObjectPath(), dbusAddressResult.getObjectPath());
    ASSERT_EQ(dbusAddressInsertRef.getInterface(), dbusAddressResult.getInterface());
}

TEST_F(AddressTranslatorTest, CheckWellKnownNameInsertWorks) {
    std::shared_ptr<CommonAPI::DBus::DBusAddressTranslator> translator = CommonAPI::DBus::DBusAddressTranslator::get();

    std::string commonApiAddressRef = "local:my.well.insert.other.interface:my.well.insert.other.instance";
    CommonAPI::DBus::DBusAddress dbusAddressInsertRef("my.well.known.name", "/my/well/insert/instance", "my.well.insert.interface");

    CommonAPI::DBus::DBusAddress dbusAddressResult;
    CommonAPI::Address commonApiAddressResult;

    translator->insert(commonApiAddressRef,
                dbusAddressInsertRef.getService(),
                dbusAddressInsertRef.getObjectPath(),
                dbusAddressInsertRef.getInterface());

    translator->translate(CommonAPI::DBus::DBusAddress(dbusAddressInsertRef.getService(), dbusAddressInsertRef.getObjectPath(), dbusAddressInsertRef.getInterface()), commonApiAddressResult);
    std::cout << dbusAddressInsertRef.getService() << " " << dbusAddressInsertRef.getObjectPath() << " " << dbusAddressInsertRef.getInterface() << std::endl;
    std::cout << commonApiAddressResult.getDomain() << " " << commonApiAddressResult.getInterface() << " " << commonApiAddressResult.getInstance() << std::endl;
    ASSERT_EQ(commonApiAddressRef, commonApiAddressResult.getAddress());

    translator->translate(commonApiAddressRef, dbusAddressResult);
    std::cout << dbusAddressResult.getService() << " " << dbusAddressResult.getObjectPath() << " " << dbusAddressResult.getInterface() << std::endl;
    ASSERT_EQ(dbusAddressInsertRef.getService(), dbusAddressResult.getService());
    ASSERT_EQ(dbusAddressInsertRef.getObjectPath(), dbusAddressResult.getObjectPath());
    ASSERT_EQ(dbusAddressInsertRef.getInterface(), dbusAddressResult.getInterface());
}

TEST_F(AddressTranslatorTest, ServicesUsingPredefinedAddressesCanCommunicate) {
    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    ASSERT_TRUE((bool)runtime);

    CommonAPI::Address commonApiAddress(commonApiAddresses[0]);
    auto defaultTestProxy = runtime->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(commonApiAddress.getDomain(), commonApiAddress.getInstance());
    ASSERT_TRUE((bool)defaultTestProxy);

    auto stub = std::make_shared<VERSION::commonapi::tests::TestInterfaceStubDefault>();

    bool serviceNameAcquired = runtime->registerService(commonApiAddress.getDomain(), commonApiAddress.getInstance(), stub, "connection");
    for(unsigned int i = 0; !serviceNameAcquired && i < 100; i++) {
        serviceNameAcquired = runtime->registerService(commonApiAddress.getDomain(), commonApiAddress.getInstance(), stub, "connection");
        usleep(10000);
    }
    ASSERT_TRUE(serviceNameAcquired);

    for(unsigned int i = 0; !defaultTestProxy->isAvailable() && i < 100; ++i) {
        usleep(10000);
    }
    ASSERT_TRUE(defaultTestProxy->isAvailable());

    uint32_t v1 = 5;
    std::string v2 = "Hai :)";
    CommonAPI::CallStatus stat;
    defaultTestProxy->testVoidPredefinedTypeMethod(v1, v2, stat);

    ASSERT_EQ(stat, CommonAPI::CallStatus::SUCCESS);
    
    runtime->unregisterService(commonApiAddress.getDomain(), stub->getStubAdapter()->getInterface(), commonApiAddress.getInstance());
}

#ifndef WIN32

const std::string domainOfFakeLegacyService = "local";
const std::string interfaceOfFakeLegacyService = "fake.legacy.service.LegacyInterface";
const std::string instanceOfFakeLegacyService = "fake.legacy.service";

const std::string domainOfFakeLegacyServiceNoObjectManager = "local";
const std::string interfaceOfFakeLegacyServiceNoObjectManager = "fake.legacy.service.LegacyInterfaceNoObjectManager";
const std::string instanceOfFakeLegacyServiceNoObjectManager = "fake.legacy.service";

TEST_F(AddressTranslatorTest, CreatedProxyHasCorrectCommonApiAddress) {
    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    ASSERT_TRUE((bool)runtime);

    auto proxyForFakeLegacyService = runtime->buildProxy<VERSION::fake::legacy::service::LegacyInterfaceProxy>(domainOfFakeLegacyService, instanceOfFakeLegacyService);
    ASSERT_TRUE((bool)proxyForFakeLegacyService);

    const CommonAPI::Address & address = proxyForFakeLegacyService->getAddress();
    ASSERT_EQ(domainOfFakeLegacyService, address.getDomain());
    ASSERT_EQ(interfaceOfFakeLegacyService, address.getInterface());
    ASSERT_EQ(instanceOfFakeLegacyService, address.getInstance());
}

void callPythonService(std::string _pythonFileNameAndCommand) {
    const char *pathToFolderForFakeLegacyService =
            getenv("TEST_COMMONAPI_DBUS_ADDRESS_TRANSLATOR_FAKE_LEGACY_SERVICE_FOLDER");

    ASSERT_NE(pathToFolderForFakeLegacyService, nullptr) << "Environment variable "
            "TEST_COMMONAPI_DBUS_ADDRESS_TRANSLATOR_FAKE_LEGACY_SERVICE_FOLDER "
            "is not set!";

    std::stringstream stream;
    stream << "python " << pathToFolderForFakeLegacyService << "/" << _pythonFileNameAndCommand;

    int resultCode = system(stream.str().c_str());
    EXPECT_EQ(0, resultCode);
}

void fakeLegacyServiceThread() {
    callPythonService("fakeLegacyService.py withObjectManager");
}

void fakeLegacyServiceThreadNoObjectMananger() {
    callPythonService("fakeLegacyService.py noObjectManager");
}

TEST_F(AddressTranslatorTest, FakeLegacyServiceCanBeAddressed) {
    std::thread fakeServiceThread = std::thread(fakeLegacyServiceThread);
    usleep(500000);

    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    ASSERT_TRUE((bool)runtime);

    auto proxyForFakeLegacyService = runtime->buildProxy<VERSION::fake::legacy::service::LegacyInterfaceProxy>(domainOfFakeLegacyService, instanceOfFakeLegacyService);
    ASSERT_TRUE((bool)proxyForFakeLegacyService);

    const CommonAPI::Address & address = proxyForFakeLegacyService->getAddress();
    ASSERT_EQ(domainOfFakeLegacyService, address.getDomain());
    ASSERT_EQ(interfaceOfFakeLegacyService, address.getInterface());
    ASSERT_EQ(instanceOfFakeLegacyService, address.getInstance());

    proxyForFakeLegacyService->isAvailableBlocking();

    CommonAPI::CallStatus status;

    const int32_t input = 42;
    int32_t output1, output2;
    proxyForFakeLegacyService->TestMethod(input, status, output1, output2);
    EXPECT_EQ(CommonAPI::CallStatus::SUCCESS, status);
    if(CommonAPI::CallStatus::SUCCESS == status) {
        EXPECT_EQ(input -5, output1);
        EXPECT_EQ(input +5, output2);
    }

    std::string greeting;
    int32_t identifier;
    proxyForFakeLegacyService->OtherTestMethod(status, greeting, identifier);
    EXPECT_EQ(CommonAPI::CallStatus::SUCCESS, status);
    if(CommonAPI::CallStatus::SUCCESS == status) {
        EXPECT_EQ(std::string("Hello"), greeting);
        EXPECT_EQ(42, identifier);
    }

    //end the fake legacy service via dbus
    callPythonService("sendToFakeLegacyService.py finish " + interfaceOfFakeLegacyService);

    fakeServiceThread.join();
}

TEST_F(AddressTranslatorTest, FakeLegacyServiceCanBeAddressedNoObjectManager) {
    std::thread fakeServiceThreadNoObjectManager = std::thread(fakeLegacyServiceThreadNoObjectMananger);
    usleep(500000);

    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    ASSERT_TRUE((bool)runtime);

    auto proxyForFakeLegacyServiceNoObjectManager = runtime->buildProxy<VERSION::fake::legacy::service::LegacyInterfaceNoObjectManagerProxy>(domainOfFakeLegacyServiceNoObjectManager, instanceOfFakeLegacyServiceNoObjectManager);
    ASSERT_TRUE((bool)proxyForFakeLegacyServiceNoObjectManager);

    const CommonAPI::Address & address = proxyForFakeLegacyServiceNoObjectManager->getAddress();
    ASSERT_EQ(domainOfFakeLegacyServiceNoObjectManager, address.getDomain());
    ASSERT_EQ(interfaceOfFakeLegacyServiceNoObjectManager, address.getInterface());
    ASSERT_EQ(instanceOfFakeLegacyServiceNoObjectManager, address.getInstance());

    proxyForFakeLegacyServiceNoObjectManager->isAvailableBlocking();

    CommonAPI::CallStatus status;

    const int32_t input = 42;
    int32_t output1, output2;
    proxyForFakeLegacyServiceNoObjectManager->TestMethod(input, status, output1, output2);
    EXPECT_EQ(CommonAPI::CallStatus::SUCCESS, status);
    if(CommonAPI::CallStatus::SUCCESS == status) {
        EXPECT_EQ(input -5, output1);
        EXPECT_EQ(input +5, output2);
    }

    std::string greeting;
    int32_t identifier;
    proxyForFakeLegacyServiceNoObjectManager->OtherTestMethod(status, greeting, identifier);
    EXPECT_EQ(CommonAPI::CallStatus::SUCCESS, status);
    if(CommonAPI::CallStatus::SUCCESS == status) {
        EXPECT_EQ(std::string("Hello"), greeting);
        EXPECT_EQ(42, identifier);
    }

    //end the fake legacy service via dbus
    callPythonService("sendToFakeLegacyService.py finish " + interfaceOfFakeLegacyServiceNoObjectManager);

    fakeServiceThreadNoObjectManager.join();
}
#endif // !WIN32

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
