// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <CommonAPI/CommonAPI.hpp>

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include <commonapi/tests/DerivedTypeCollection.hpp>
#include <v1/commonapi/tests/TestFreedesktopInterfaceProxy.hpp>
#include <v1/commonapi/tests/TestFreedesktopInterfaceStub.hpp>
#include <v1/commonapi/tests/TestFreedesktopInterfaceStubDefault.hpp>
#include <v1/commonapi/tests/TestFreedesktopDerivedInterfaceProxy.hpp>
#include <v1/commonapi/tests/TestFreedesktopDerivedInterfaceStubDefault.hpp>

#define VERSION v1_0

static const std::string domain = "local";
static const std::string commonApiAddress = "commonapi.tests.TestFreedesktopInterface";
static const std::string commonApiDerivedAddress = "commonapi.tests.TestFreedesktopDerivedInterface";

class FreedesktopPropertiesTest: public ::testing::Test {
protected:
    void SetUp() {
        runtime = CommonAPI::Runtime::get();

        proxy_ = runtime->buildProxy<VERSION::commonapi::tests::TestFreedesktopInterfaceProxy>(domain, commonApiAddress, "client");

        registerTestStub();

        for (unsigned int i = 0; !proxy_->isAvailable() && i < 100; ++i) {
            usleep(10000);
        }
        ASSERT_TRUE(proxy_->isAvailable());
    }

    virtual void TearDown() {
        deregisterTestStub();
        usleep(30000);
    }

    void registerTestStub() {
        testStub_ = std::make_shared<VERSION::commonapi::tests::TestFreedesktopInterfaceStubDefault>();
        const bool isServiceRegistered = runtime->registerService(domain, commonApiAddress, testStub_, "connection");

        ASSERT_TRUE(isServiceRegistered);
    }


    void deregisterTestStub() {
        const bool isStubAdapterUnregistered = runtime->unregisterService(domain, testStub_->getStubAdapter()->getInterface(), commonApiAddress);
        ASSERT_TRUE(isStubAdapterUnregistered);
    }

    std::shared_ptr<CommonAPI::Runtime> runtime;
    std::shared_ptr<VERSION::commonapi::tests::TestFreedesktopInterfaceProxy<>> proxy_;
    std::shared_ptr<VERSION::commonapi::tests::TestFreedesktopInterfaceStubDefault> testStub_;
};

TEST_F(FreedesktopPropertiesTest, GetBasicTypeAttribute) {
    CommonAPI::CallStatus callStatus(CommonAPI::CallStatus::REMOTE_ERROR);
    uint32_t value;

    auto& testAttribute = proxy_->getTestPredefinedTypeAttributeAttribute();

    testAttribute.getValue(callStatus, value);

    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
}

TEST_F(FreedesktopPropertiesTest, GetAndSetBasicTypeAttribute) {
    CommonAPI::CallStatus callStatus(CommonAPI::CallStatus::REMOTE_ERROR);
    uint32_t value;

    auto& testAttribute = proxy_->getTestPredefinedTypeAttributeAttribute();

    testAttribute.getValue(callStatus, value);

    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);

    callStatus = CommonAPI::CallStatus::REMOTE_ERROR;
    uint32_t newValue = 7;

    testAttribute.setValue(newValue, callStatus, value);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(value, 7);

    value = 0;
    callStatus = CommonAPI::CallStatus::REMOTE_ERROR;
    testAttribute.getValue(callStatus, value);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(value, 7);
}

TEST_F(FreedesktopPropertiesTest, CanSendAndReceiveNotificationForSingleProperty) {
    auto& testAttribute = proxy_->getTestPredefinedTypeAttributeAttribute();

    bool callbackArrived = false;
    bool initialCall = true;

    std::function<void(const uint32_t)> listener([&](const uint32_t value) {
        // the first call is for the initial value. Ignore it.
        if (initialCall) {
            initialCall = false;
        } else {
            ASSERT_EQ(8, value);
            callbackArrived = true;
        }
    });

    usleep(200000);

    testAttribute.getChangedEvent().subscribe(listener);

    CommonAPI::CallStatus callStatus = CommonAPI::CallStatus::REMOTE_ERROR;
    uint32_t value;
    uint32_t newValue = 8;

    testAttribute.setValue(newValue, callStatus, value);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(value, 8);


    uint8_t waitCounter = 0;
    while(!callbackArrived && waitCounter < 10) {
        usleep(50000);
        waitCounter++;
    }

    ASSERT_TRUE(callbackArrived);
}

class FreedesktopPropertiesOnInheritedInterfacesTest: public ::testing::Test {
protected:
    void SetUp() {
        runtime = CommonAPI::Runtime::get();

        proxy_ = runtime->buildProxy<VERSION::commonapi::tests::TestFreedesktopDerivedInterfaceProxy>(domain, commonApiDerivedAddress, "client");

        registerTestStub();

        for (unsigned int i = 0; !proxy_->isAvailable() && i < 100; ++i) {
            usleep(10000);
        }
        ASSERT_TRUE(proxy_->isAvailable());
    }

    virtual void TearDown() {
        deregisterTestStub();
        usleep(30000);
    }

    void registerTestStub() {
        testStub_ = std::make_shared<VERSION::commonapi::tests::TestFreedesktopDerivedInterfaceStubDefault>();
        const bool isServiceRegistered = runtime->registerService(domain, commonApiDerivedAddress, testStub_, "connection");

        ASSERT_TRUE(isServiceRegistered);
    }

    void deregisterTestStub() {
        const bool isStubAdapterUnregistered = runtime->unregisterService(domain, testStub_->CommonAPI::Stub<VERSION::commonapi::tests::TestFreedesktopDerivedInterfaceStubAdapter, VERSION::commonapi::tests::TestFreedesktopDerivedInterfaceStubRemoteEvent>::getStubAdapter()->VERSION::commonapi::tests::TestFreedesktopDerivedInterface::getInterface(), commonApiDerivedAddress);
        ASSERT_TRUE(isStubAdapterUnregistered);
    }


    std::shared_ptr<CommonAPI::Runtime> runtime;
    std::shared_ptr<VERSION::commonapi::tests::TestFreedesktopDerivedInterfaceProxy<>> proxy_;
    std::shared_ptr<VERSION::commonapi::tests::TestFreedesktopDerivedInterfaceStubDefault> testStub_;
};

TEST_F(FreedesktopPropertiesOnInheritedInterfacesTest, CanGetAndSetRemoteAttributeFromDerivedInterface) {
    auto& testAttribute = proxy_->getTestAttributedFromDerivedInterfaceAttribute();

    CommonAPI::CallStatus callStatus(CommonAPI::CallStatus::REMOTE_ERROR);
    uint32_t value;
    testAttribute.getValue(callStatus, value);

    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);

    callStatus = CommonAPI::CallStatus::REMOTE_ERROR;
    uint32_t newValue = 7;

    testAttribute.setValue(newValue, callStatus, value);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(value, 7);

    value = 0;
    callStatus = CommonAPI::CallStatus::REMOTE_ERROR;
    testAttribute.getValue(callStatus, value);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(value, 7);
}

TEST_F(FreedesktopPropertiesOnInheritedInterfacesTest, CanGetAndSetRemoteAttributeFromParentInterface) {
    auto& testAttribute = proxy_->getTestPredefinedTypeAttributeAttribute();

    CommonAPI::CallStatus callStatus(CommonAPI::CallStatus::REMOTE_ERROR);
    uint32_t value;
    testAttribute.getValue(callStatus, value);

    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);

    callStatus = CommonAPI::CallStatus::REMOTE_ERROR;
    uint32_t newValue = 7;

    testAttribute.setValue(newValue, callStatus, value);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(value, 7);

    value = 0;
    callStatus = CommonAPI::CallStatus::REMOTE_ERROR;
    testAttribute.getValue(callStatus, value);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(value, 7);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
