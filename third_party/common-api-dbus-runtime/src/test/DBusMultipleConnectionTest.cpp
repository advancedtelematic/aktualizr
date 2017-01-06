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
#include <utility>
#include <tuple>
#include <type_traits>

#include <CommonAPI/CommonAPI.hpp>

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#define VERSION v1_0

#include "v1/commonapi/tests/TestInterfaceProxy.hpp"
#include "v1/commonapi/tests/TestInterfaceStubDefault.hpp"

const std::string domain = "local";
const std::string serviceAddress = "commonapi.tests.TestInterface";

class DBusMultipleConnectionTest: public ::testing::Test {
 protected:
     virtual void SetUp() {
        runtime = CommonAPI::Runtime::get();
        stub = std::make_shared<VERSION::commonapi::tests::TestInterfaceStubDefault>();
        bool serviceNameAcquired = runtime->registerService(domain, serviceAddress, stub, "connection");

        for(unsigned int i = 0; !serviceNameAcquired && i < 100; i++) {
            usleep(10000);
            serviceNameAcquired = runtime->registerService(domain, serviceAddress, stub, "connection");
        }
        ASSERT_TRUE(serviceNameAcquired);

        proxy = runtime->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(domain, serviceAddress);
        ASSERT_TRUE((bool)proxy);

        for(unsigned int i = 0; !proxy->isAvailable() && i < 100; ++i) {
            usleep(10000);
        }
    }

    virtual void TearDown() {
        runtime->unregisterService(domain, stub->getStubAdapter()->getInterface(), serviceAddress);
        usleep(30000);
    }

    std::shared_ptr<CommonAPI::Runtime> runtime;
    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceStubDefault> stub;
    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceProxy<>> proxy;
};


TEST_F(DBusMultipleConnectionTest, RemoteMethodCall) {
    uint32_t v1 = 5;
    std::string v2 = "Hai :)";
    CommonAPI::CallStatus stat;
    proxy->testVoidPredefinedTypeMethod(v1, v2, stat);
    ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, stat);
}

TEST_F(DBusMultipleConnectionTest, Broadcast) {
    uint32_t v1 = 5;
    uint32_t v3 = 0;
    std::string v2 = "Hai :)";

    std::promise<bool> promise;
    auto future = promise.get_future();

    auto subscription = proxy->getTestPredefinedTypeBroadcastEvent().subscribe([&](
                    const uint32_t intVal, const std::string& strVal) {
        (void)strVal;
        v3 = intVal;
        promise.set_value(true);
    });

    stub->fireTestPredefinedTypeBroadcastEvent(v1, v2);

    ASSERT_TRUE(future.get());
    ASSERT_EQ(v1, v3);

    proxy->getTestPredefinedTypeBroadcastEvent().unsubscribe(subscription);
}

TEST_F(DBusMultipleConnectionTest, SetAttribute) {
    uint32_t v1 = 5;
    uint32_t v2;
    CommonAPI::CallStatus stat;
    proxy->getTestPredefinedTypeAttributeAttribute().setValue(v1, stat, v2);
    ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, stat);
    ASSERT_EQ(v1, v2);
}

TEST_F(DBusMultipleConnectionTest, SetAttributeBroadcast) {
    uint32_t v1 = 6;
    uint32_t v2;
    uint32_t v3 = 0;
    bool initial = true;

    std::promise<bool> promise;
    auto future = promise.get_future();

    auto subscription = proxy->getTestPredefinedTypeAttributeAttribute().getChangedEvent().subscribe([&](
                    const uint32_t intVal) {
        // this subscription is called twice. Ignore the first call (the initial value).
        if (initial) {
            initial = false;
            return;
        }
        v3 = intVal;
        promise.set_value(true);
    });

    CommonAPI::CallStatus stat;
    proxy->getTestPredefinedTypeAttributeAttribute().setValue(v1, stat, v2);
    ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, stat);
    ASSERT_EQ(v1, v2);

    ASSERT_TRUE(future.get());
    ASSERT_EQ(v1, v3);

    proxy->getTestPredefinedTypeAttributeAttribute().getChangedEvent().unsubscribe(subscription);
}


TEST_F(DBusMultipleConnectionTest, GetAttribute) {
    uint32_t v1;
    CommonAPI::CallStatus status;
    proxy->getTestPredefinedTypeAttributeAttribute().getValue(status, v1);
    ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, status);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
