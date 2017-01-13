// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include "CommonAPI/CommonAPI.hpp"

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include "CommonAPI/DBus/DBusConnection.hpp"

#define VERSION v1_0

#include <commonapi/tests/DerivedTypeCollection.hpp>
#include <v1/commonapi/tests/TestInterfaceDBusProxy.hpp>
#include <v1/commonapi/tests/TestInterfaceDBusStubAdapter.hpp>
#include <v1/commonapi/tests/TestInterfaceStubDefault.hpp>

static const std::string interfaceName = "commonapi.tests.TestInterface";
static const std::string busName = "commonapi.tests.TestInterface_CommonAPI.DBus.tests.DBusProxyTestService";
static const std::string objectPath = "/CommonAPI/DBus/tests/DBusProxyTestService";

class PolymorphicTestStub : public VERSION::commonapi::tests::TestInterfaceStubDefault {
public:

    void TestArrayOfPolymorphicStructMethod(const std::shared_ptr<CommonAPI::ClientId> _client,
        std::vector<std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestPolymorphicStruct>> inArray,
        TestArrayOfPolymorphicStructMethodReply_t _reply) {
        (void)_client;

        numberOfContainedElements_ = static_cast<int>(inArray.size());

        if (numberOfContainedElements_ > 0) {
            std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct> extended =
                            std::dynamic_pointer_cast<
                                            ::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct>(
                                            inArray[0]);
            firstElementIsExtended_ = (extended != NULL);
        }

        if (numberOfContainedElements_ > 1) {
            std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct> extended =
                            std::dynamic_pointer_cast<
                                            ::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct>(
                                            inArray[1]);
            secondElementIsExtended_ = (extended != NULL);
        }
        _reply();
    }

                   
    void TestMapOfPolymorphicStructMethod(
        const std::shared_ptr<CommonAPI::ClientId> clientId, 
        ::commonapi::tests::DerivedTypeCollection::MapIntToPolymorphic inMap, 
        TestMapOfPolymorphicStructMethodReply_t _reply) {
        (void)clientId;

        numberOfContainedElements_ = static_cast<int>(inMap.size());

        auto iteratorFirst = inMap.find(5);
        auto iteratorSecond = inMap.find(10);

        if (iteratorFirst != inMap.end()) {
            std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct> extended =
                            std::dynamic_pointer_cast<
                                            ::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct>(
                                            iteratorFirst->second);
            firstElementIsExtended_ = (extended != NULL);
        }

        if (iteratorSecond != inMap.end()) {
            std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct> extended =
                            std::dynamic_pointer_cast<
                                            ::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct>(
                                            iteratorSecond->second);
            secondElementIsExtended_ = (extended != NULL);
        }

        _reply();
    }

    void TestStructWithPolymorphicMemberMethod(
        const std::shared_ptr<CommonAPI::ClientId> _client, 
        ::commonapi::tests::DerivedTypeCollection::StructWithPolymorphicMember inStruct, 
        TestStructWithPolymorphicMemberMethodReply_t _reply) {
        (void)_client;

        if (inStruct.getPolymorphicMember() != NULL) {
            numberOfContainedElements_ = 1;

            std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct> extended =
                            std::dynamic_pointer_cast<
                                            ::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct>(
                                            inStruct.getPolymorphicMember());
            firstElementIsExtended_ = (extended != NULL);
        }
        _reply();
    }

    int numberOfContainedElements_;
    bool firstElementIsExtended_;
    bool secondElementIsExtended_;
};

class PolymorphicTest: public ::testing::Test {
protected:
    void SetUp() {
        auto runtime = CommonAPI::Runtime::get();

        proxyDBusConnection_ = CommonAPI::DBus::DBusConnection::getBus(CommonAPI::DBus::DBusType_t::SESSION, "clientConnection");
        ASSERT_TRUE(proxyDBusConnection_->connect());

        proxy_ = std::make_shared<VERSION::commonapi::tests::TestInterfaceDBusProxy>(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), proxyDBusConnection_);
        proxy_->init();

        registerTestStub();

        for (unsigned int i = 0; !proxy_->isAvailable() && i < 100; ++i) {
            usleep(10000);
        }
        ASSERT_TRUE(proxy_->isAvailable());

        baseInstance1_ = std::make_shared<::commonapi::tests::DerivedTypeCollection::TestPolymorphicStruct>();
        baseInstance1_->setTestString("abc");

        extendedInstance1_ = std::make_shared<::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct>();
        extendedInstance1_->setAdditionalValue(7);
    }

    virtual void TearDown() {
        deregisterTestStub();
        usleep(30000);
    }

    void registerTestStub() {
        stubDBusConnection_ = CommonAPI::DBus::DBusConnection::getBus(CommonAPI::DBus::DBusType_t::SESSION, "serviceConnection");
        ASSERT_TRUE(stubDBusConnection_->connect());

        testStub = std::make_shared<PolymorphicTestStub>();
        stubAdapter_ = std::make_shared<VERSION::commonapi::tests::TestInterfaceDBusStubAdapter>(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), stubDBusConnection_, testStub);
        stubAdapter_->init(stubAdapter_);

        const bool isStubAdapterRegistered = CommonAPI::Runtime::get()->registerService(
            stubAdapter_->getAddress().getDomain(), stubAdapter_->getAddress().getInstance(), testStub);
        ASSERT_TRUE(isStubAdapterRegistered);
    }

    void deregisterTestStub() {
        const bool isStubAdapterUnregistered = CommonAPI::Runtime::get()->unregisterService(stubAdapter_->getAddress().getDomain(), stubAdapter_->getInterface(), stubAdapter_->getAddress().getInstance());
        ASSERT_TRUE(isStubAdapterUnregistered);
        stubAdapter_.reset();

        if (stubDBusConnection_->isConnected()) {
            stubDBusConnection_->disconnect();
        }
        stubDBusConnection_.reset();
    }

    std::shared_ptr<CommonAPI::DBus::DBusConnection> proxyDBusConnection_;
    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceDBusProxy> proxy_;

    std::shared_ptr<CommonAPI::DBus::DBusConnection> stubDBusConnection_;
    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceDBusStubAdapter> stubAdapter_;

    std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestPolymorphicStruct> baseInstance1_;
    std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestExtendedPolymorphicStruct> extendedInstance1_;

    std::shared_ptr<PolymorphicTestStub> testStub;
};

TEST_F(PolymorphicTest, SendVectorOfBaseType) {
    CommonAPI::CallStatus stat;
    std::vector<std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestPolymorphicStruct> > inputArray;
    inputArray.push_back(baseInstance1_);

    proxy_->TestArrayOfPolymorphicStructMethod(inputArray, stat, nullptr);

    ASSERT_EQ(stat, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(testStub->numberOfContainedElements_, 1);
    ASSERT_FALSE(testStub->firstElementIsExtended_);
}

TEST_F(PolymorphicTest, SendVectorOfExtendedType) {
    CommonAPI::CallStatus stat;
    std::vector<std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestPolymorphicStruct> > inputArray;
    inputArray.push_back(extendedInstance1_);

    proxy_->TestArrayOfPolymorphicStructMethod(inputArray, stat, nullptr);

    ASSERT_EQ(stat, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(testStub->numberOfContainedElements_, 1);
    ASSERT_TRUE(testStub->firstElementIsExtended_);
}

TEST_F(PolymorphicTest, SendVectorOfBaseAndExtendedType) {
    CommonAPI::CallStatus stat;
    std::vector<std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestPolymorphicStruct> > inputArray;
    inputArray.push_back(baseInstance1_);
    inputArray.push_back(extendedInstance1_);

    proxy_->TestArrayOfPolymorphicStructMethod(inputArray, stat, nullptr);

    ASSERT_EQ(stat, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(testStub->numberOfContainedElements_, 2);
    ASSERT_FALSE(testStub->firstElementIsExtended_);
    ASSERT_TRUE(testStub->secondElementIsExtended_);
}

TEST_F(PolymorphicTest, SendMapOfBaseAndExtendedType) {
    CommonAPI::CallStatus stat;
    std::unordered_map<uint8_t, std::shared_ptr<::commonapi::tests::DerivedTypeCollection::TestPolymorphicStruct> > inputMap;
    inputMap.insert( {5, baseInstance1_});
    inputMap.insert( {10, extendedInstance1_});

    proxy_->TestMapOfPolymorphicStructMethod(inputMap, stat, nullptr);

    ASSERT_EQ(stat, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(testStub->numberOfContainedElements_, 2);
    ASSERT_FALSE(testStub->firstElementIsExtended_);
    ASSERT_TRUE(testStub->secondElementIsExtended_);
}

TEST_F(PolymorphicTest, SendStructWithPolymorphicMember) {
    CommonAPI::CallStatus stat;
    ::commonapi::tests::DerivedTypeCollection::StructWithPolymorphicMember inputStruct;
    inputStruct.setPolymorphicMember(extendedInstance1_);

    proxy_->TestStructWithPolymorphicMemberMethod(inputStruct, stat, nullptr);

    ASSERT_EQ(stat, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(testStub->numberOfContainedElements_, 1);
    ASSERT_TRUE(testStub->firstElementIsExtended_);
}

TEST_F(PolymorphicTest, SendStructWithMapWithEnumKeyMember) {
    CommonAPI::CallStatus stat;
    ::commonapi::tests::DerivedTypeCollection::StructWithEnumKeyMap inputStruct;
    std::get<0>(inputStruct.values_).insert( { commonapi::tests::DerivedTypeCollection::TestEnum::E_OK, "test" } );

    proxy_->TestStructWithEnumKeyMapMember(inputStruct, stat, nullptr);

    ASSERT_EQ(stat, CommonAPI::CallStatus::SUCCESS);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
