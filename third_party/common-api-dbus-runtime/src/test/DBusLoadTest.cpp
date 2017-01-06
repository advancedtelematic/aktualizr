// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <array>
#include <utility>
#include <tuple>
#include <type_traits>
#include <future>

#include <CommonAPI/CommonAPI.hpp>

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>

#include "commonapi/tests/PredefinedTypeCollection.hpp"
#include "commonapi/tests/DerivedTypeCollection.hpp"
#include "v1/commonapi/tests/TestInterfaceProxy.hpp"
#include "v1/commonapi/tests/TestInterfaceStubDefault.hpp"

#include "v1/commonapi/tests/TestInterfaceDBusProxy.hpp"

#define VERSION v1_0

class TestInterfaceStubFinal : public VERSION::commonapi::tests::TestInterfaceStubDefault {

public:
    void testPredefinedTypeMethod(const std::shared_ptr<CommonAPI::ClientId> _client, 
                                    uint32_t _uint32InValue, 
                                    std::string _stringInValue, 
                                    testPredefinedTypeMethodReply_t _reply) {
        (void)_client;
        uint32_t uint32OutValue = _uint32InValue;
        std::string stringOutValue = _stringInValue;
        _reply(uint32OutValue, stringOutValue);
    }
};

class DBusLoadTest: public ::testing::Test {
protected:
    virtual void SetUp() {
        runtime_ = CommonAPI::Runtime::get();
        ASSERT_TRUE((bool )runtime_);

        callSucceeded_.resize(numCallsPerProxy_ * numProxies_);
        std::fill(callSucceeded_.begin(), callSucceeded_.end(), false);
    }

    virtual void TearDown() {
        usleep(1000000);
    }

public:
    void TestPredefinedTypeMethodAsyncCallback(
                                               const uint32_t callId,
                                               const uint32_t in1,
                                               const std::string in2,
                                               const CommonAPI::CallStatus& callStatus,
                                               const uint32_t& out1,
                                               const std::string& out2) {
        EXPECT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
        EXPECT_EQ(out1, in1);
        EXPECT_EQ(out2, in2);
        mutexCallSucceeded_.lock();
        ASSERT_FALSE(callSucceeded_[callId]);
        callSucceeded_[callId] = true;
        mutexCallSucceeded_.unlock();
    }

    std::shared_ptr<CommonAPI::Runtime> runtime_;
    std::vector<bool> callSucceeded_;
    std::mutex mutexCallSucceeded_;

    static const std::string domain_;
    static const std::string serviceAddress_;
    static const uint32_t numCallsPerProxy_;
    static const uint32_t numProxies_;
};

const std::string DBusLoadTest::domain_ = "local";
const std::string DBusLoadTest::serviceAddress_ = "CommonAPI.DBus.tests.DBusProxyTestService";
const uint32_t DBusLoadTest::numCallsPerProxy_ = 100;

#ifdef WIN32
// test with just 50 proxies under windows as it becomes very slow with more ones
const uint32_t DBusLoadTest::numProxies_ = 50;
#else
const uint32_t DBusLoadTest::numProxies_ = 65;
#endif

// Multiple proxies in one thread, one stub
TEST_F(DBusLoadTest, SingleClientMultipleProxiesSingleStubCallsSucceed) {
    std::array<std::shared_ptr<VERSION::commonapi::tests::TestInterfaceProxyBase>, numProxies_> testProxies;

    for (unsigned int i = 0; i < numProxies_; i++) {
        testProxies[i] = runtime_->buildProxy < VERSION::commonapi::tests::TestInterfaceProxy >(domain_, serviceAddress_);
        ASSERT_TRUE((bool )testProxies[i]);
    }

    auto stub = std::make_shared<TestInterfaceStubFinal>();
    bool serviceRegistered = runtime_->registerService(domain_, serviceAddress_, stub, "connection");
    for (auto i = 0; !serviceRegistered && i < 100; ++i) {
        serviceRegistered = runtime_->registerService(domain_, serviceAddress_, stub, "connection");
        usleep(10000);
    }
    ASSERT_TRUE(serviceRegistered);

    bool allProxiesAvailable = false;
    for (unsigned int i = 0; !allProxiesAvailable && i < 100; ++i) {
        allProxiesAvailable = true;
        for (unsigned int j = 0; j < numProxies_; ++j) {
            allProxiesAvailable = allProxiesAvailable && testProxies[j]->isAvailable();
        }
        if (!allProxiesAvailable)
            usleep(10000);
    }
    ASSERT_TRUE(allProxiesAvailable);

    uint32_t callId = 0;
    for (unsigned int i = 0; i < numCallsPerProxy_; i++) {
        for (unsigned int j = 0; j < numProxies_; j++) {
            uint32_t in1 = i;
            std::string in2 = "string" + std::to_string(i) + "_" + std::to_string(j);
            testProxies[j]->testPredefinedTypeMethodAsync(
                            in1,
                            in2,
                            std::bind(
                                      &DBusLoadTest::TestPredefinedTypeMethodAsyncCallback,
                                      this,
                                      callId++,
                                      in1,
                                      in2,
                                      std::placeholders::_1,
                                      std::placeholders::_2,
                                      std::placeholders::_3));
        }
    }

    bool allCallsSucceeded = false;
    for (unsigned int i = 0; !allCallsSucceeded && i < 100; ++i) {
        allCallsSucceeded = std::all_of(callSucceeded_.cbegin(), callSucceeded_.cend(), [](int b){ return b; });
        if (!allCallsSucceeded)
            usleep(100000);
    }
    ASSERT_TRUE(allCallsSucceeded);

    runtime_->unregisterService(domain_, stub->getStubAdapter()->getInterface(), serviceAddress_);
}

// Multiple proxies in separate threads, one stub
TEST_F(DBusLoadTest, MultipleClientsSingleStubCallsSucceed) {
    std::array<std::shared_ptr<CommonAPI::Factory>, numProxies_> testProxyFactories;
    std::array<std::shared_ptr<VERSION::commonapi::tests::TestInterfaceProxyBase>, numProxies_> testProxies;

    for (unsigned int i = 0; i < numProxies_; i++) {
        testProxies[i] = runtime_->buildProxy < VERSION::commonapi::tests::TestInterfaceProxy >(domain_, serviceAddress_);
        ASSERT_TRUE((bool )testProxies[i]);
    }

    auto stub = std::make_shared<TestInterfaceStubFinal>();
    bool serviceRegistered = false;
    for (auto i = 0; !serviceRegistered && i < 100; ++i) {
        serviceRegistered = runtime_->registerService(domain_, serviceAddress_, stub, "connection");
        if(!serviceRegistered)
            usleep(10000);
    }
    ASSERT_TRUE(serviceRegistered);

    bool allProxiesAvailable = false;
    for (unsigned int i = 0; !allProxiesAvailable && i < 100; ++i) {
        allProxiesAvailable = true;
        for (unsigned int j = 0; j < numProxies_; ++j) {
            allProxiesAvailable = allProxiesAvailable && testProxies[j]->isAvailable();
        }
        if (!allProxiesAvailable)
            usleep(10000);
    }
    ASSERT_TRUE(allProxiesAvailable);

    uint32_t callId = 0;
    for (unsigned int i = 0; i < numCallsPerProxy_; i++) {
        for (unsigned int j = 0; j < numProxies_; j++) {
            uint32_t in1 = i;
            std::string in2 = "string" + std::to_string(i) + "_" + std::to_string(j);
            testProxies[j]->testPredefinedTypeMethodAsync(
                            in1,
                            in2,
                            std::bind(
                                      &DBusLoadTest::TestPredefinedTypeMethodAsyncCallback,
                                      this,
                                      callId++,
                                      in1,
                                      in2,
                                      std::placeholders::_1,
                                      std::placeholders::_2,
                                      std::placeholders::_3));
        }
    }

    bool allCallsSucceeded = false;
    for (unsigned int i = 0; !allCallsSucceeded && i < 100; ++i) {
        allCallsSucceeded = std::all_of(callSucceeded_.cbegin(), callSucceeded_.cend(), [](int b){ return b; });
        if (!allCallsSucceeded)
            usleep(100000);
    }
    ASSERT_TRUE(allCallsSucceeded);

    runtime_->unregisterService(domain_, stub->getStubAdapter()->getInterface(), serviceAddress_);
}

// Multiple proxies in separate threads, multiple stubs in separate threads
TEST_F(DBusLoadTest, MultipleClientsMultipleServersCallsSucceed) {
    std::array<std::shared_ptr<CommonAPI::Factory>, numProxies_> testProxyFactories;
    std::array<std::shared_ptr<CommonAPI::Factory>, numProxies_> testStubFactories;
    std::array<std::shared_ptr<VERSION::commonapi::tests::TestInterfaceProxyBase>, numProxies_> testProxies;
    std::array<std::shared_ptr<VERSION::commonapi::tests::TestInterfaceStub>, numProxies_> testStubs;

    for (unsigned int i = 0; i < numProxies_; i++) {
        testProxies[i] = runtime_->buildProxy < VERSION::commonapi::tests::TestInterfaceProxy >(domain_, serviceAddress_ + std::to_string(i));
        ASSERT_TRUE((bool )testProxies[i]);
    }

    for (unsigned int i = 0; i < numProxies_; i++) {
        testStubs[i] = std::make_shared<TestInterfaceStubFinal>();
        ASSERT_TRUE((bool )testStubs[i]);
        bool serviceRegistered = false;
        for (auto j = 0; !serviceRegistered && j < 100; ++j) {
            serviceRegistered = runtime_->registerService(domain_, serviceAddress_ + std::to_string(i), testStubs[i], "connection");
            if(!serviceRegistered)
                usleep(10000);
        }
        ASSERT_TRUE(serviceRegistered);
    }

    bool allProxiesAvailable = false;
    for (unsigned int i = 0; !allProxiesAvailable && i < 100; ++i) {
        allProxiesAvailable = true;
        for (unsigned int j = 0; j < numProxies_; ++j) {
            allProxiesAvailable = allProxiesAvailable && testProxies[j]->isAvailable();
        }
        if (!allProxiesAvailable)
            usleep(1000000);
    }
    ASSERT_TRUE(allProxiesAvailable);

    uint32_t callId = 0;
    for (unsigned int i = 0; i < numCallsPerProxy_; i++) {
        for (unsigned int j = 0; j < numProxies_; j++) {
            uint32_t in1 = i;
            std::string in2 = "string" + std::to_string(i) + "_" + std::to_string(j);
            testProxies[j]->testPredefinedTypeMethodAsync(
                            in1,
                            in2,
                            std::bind(
                                      &DBusLoadTest::TestPredefinedTypeMethodAsyncCallback,
                                      this,
                                      callId++,
                                      in1,
                                      in2,
                                      std::placeholders::_1,
                                      std::placeholders::_2,
                                      std::placeholders::_3));
        }
    }

    bool allCallsSucceeded = false;
    for (auto i = 0; !allCallsSucceeded && i < 100; ++i) {
        allCallsSucceeded = std::all_of(callSucceeded_.cbegin(), callSucceeded_.cend(), [](int b){ return b; });
        if (!allCallsSucceeded)
            usleep(100000);
    }
    ASSERT_TRUE(allCallsSucceeded);

    for (unsigned int i = 0; i < numProxies_; i++) {
        runtime_->unregisterService(domain_, testStubs[i]->getStubAdapter()->getInterface(), serviceAddress_ + std::to_string(i));
    }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
