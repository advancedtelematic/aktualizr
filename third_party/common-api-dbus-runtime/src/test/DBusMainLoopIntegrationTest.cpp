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
#ifndef WIN32
#include <glib.h>
#endif

#include <CommonAPI/CommonAPI.hpp>

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>

#include "DBusTestUtils.hpp"
#include "DemoMainLoop.hpp"

#include "commonapi/tests/PredefinedTypeCollection.hpp"
#include "commonapi/tests/DerivedTypeCollection.hpp"
#include "v1/commonapi/tests/TestInterfaceProxy.hpp"
#include "v1/commonapi/tests/TestInterfaceStubDefault.hpp"
#include "v1/commonapi/tests/TestInterfaceDBusStubAdapter.hpp"

#include "v1/commonapi/tests/TestInterfaceDBusProxy.hpp"

const std::string domain = "local";
const std::string testAddress1 = "commonapi.address.one";
const std::string testAddress2 = "commonapi.address.two";
const std::string testAddress3 = "commonapi.address.three";
const std::string testAddress4 = "commonapi.address.four";
const std::string testAddress5 = "commonapi.address.five";
const std::string testAddress6 = "commonapi.address.six";
const std::string testAddress7 = "commonapi.address.seven";
const std::string testAddress8 = "commonapi.address.eight";

#define VERSION v1_0

//####################################################################################################################

class DBusBasicMainLoopTest: public ::testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

struct TestSource: public CommonAPI::DispatchSource {
    TestSource(const std::string value, std::string& result): value_(value), result_(result) {}

    bool prepare(int64_t& timeout) {
        (void)timeout;
        return true;
    }
    bool check() {
        return true;
    }
    bool dispatch() {
        result_.append(value_);
        return false;
    }

 private:
    std::string value_;
    std::string& result_;
};

TEST_F(DBusBasicMainLoopTest, PrioritiesAreHandledCorrectlyInDemoMainloop) {
    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    ASSERT_TRUE((bool) runtime);

    std::shared_ptr<CommonAPI::MainLoopContext> context = std::make_shared<CommonAPI::MainLoopContext>();
    ASSERT_TRUE((bool) context);

    auto mainLoop = new CommonAPI::MainLoop(context);

    std::string result = "";

    TestSource* testSource1Default = new TestSource("A", result);
    TestSource* testSource2Default = new TestSource("B", result);
    TestSource* testSource1High = new TestSource("C", result);
    TestSource* testSource1Low = new TestSource("D", result);
    TestSource* testSource1VeryHigh = new TestSource("E", result);
    context->registerDispatchSource(testSource1Default);
    context->registerDispatchSource(testSource2Default, CommonAPI::DispatchPriority::DEFAULT);
    context->registerDispatchSource(testSource1High, CommonAPI::DispatchPriority::HIGH);
    context->registerDispatchSource(testSource1Low, CommonAPI::DispatchPriority::LOW);
    context->registerDispatchSource(testSource1VeryHigh, CommonAPI::DispatchPriority::VERY_HIGH);

    mainLoop->wakeup();
    mainLoop->doSingleIteration(CommonAPI::TIMEOUT_INFINITE);

    std::string reference1("ECABD");
    std::string reference2("ECBAD");
    ASSERT_TRUE(reference1 == result || reference2 == result);
}


//####################################################################################################################

class DBusMainLoopTest: public ::testing::Test {
protected:
    virtual void SetUp() {
        runtime_ = CommonAPI::Runtime::get();
        ASSERT_TRUE((bool) runtime_);

        context_ = std::make_shared<CommonAPI::MainLoopContext>();
        ASSERT_TRUE((bool) context_);
        mainLoop_ = new CommonAPI::MainLoop(context_);
    }

    virtual void TearDown() {
        delete mainLoop_;
    }

    std::shared_ptr<CommonAPI::Runtime> runtime_;
    std::shared_ptr<CommonAPI::MainLoopContext> context_;
    CommonAPI::MainLoop* mainLoop_;
};


TEST_F(DBusMainLoopTest, ServiceInDemoMainloopCanBeAddressed) {
    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceStubDefault> stub = std::make_shared<
        VERSION::commonapi::tests::TestInterfaceStubDefault>();
    ASSERT_TRUE(runtime_->registerService(domain, testAddress1, stub, context_));

    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(domain, testAddress1, "connection");
    ASSERT_TRUE((bool) proxy);

    while (!proxy->isAvailable()) {
        mainLoop_->doSingleIteration(50000);
    }

    uint32_t uint32Value = 42;
    std::string stringValue = "Hai :)";

    std::future<CommonAPI::CallStatus> futureStatus = proxy->testVoidPredefinedTypeMethodAsync(
                    uint32Value,
                    stringValue,
                    [&] (const CommonAPI::CallStatus& status) {
                        EXPECT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(status));
                        mainLoop_->stop();
                    }
    );

    mainLoop_->run();

    ASSERT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(futureStatus.get()));

    runtime_->unregisterService(domain, stub->getStubAdapter()->getInterface(), testAddress1);
}


TEST_F(DBusMainLoopTest, ProxyInDemoMainloopCanCallMethods) {
    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceStubDefault> stub = std::make_shared<
        VERSION::commonapi::tests::TestInterfaceStubDefault>();
    ASSERT_TRUE(runtime_->registerService(domain, testAddress2, stub, "connection"));

    usleep(500000);

    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(domain, testAddress2, context_);
    ASSERT_TRUE((bool) proxy);

    while (!proxy->isAvailable()) {
        mainLoop_->doSingleIteration();
        usleep(50000);
    }

    uint32_t uint32Value = 42;
    std::string stringValue = "Hai :)";

    std::future<CommonAPI::CallStatus> futureStatus = proxy->testVoidPredefinedTypeMethodAsync(
                    uint32Value,
                    stringValue,
                    [&] (const CommonAPI::CallStatus& status) {
                        EXPECT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(status));
                        mainLoop_->stop();
                    }
    );

    mainLoop_->run();

    ASSERT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(futureStatus.get()));

    runtime_->unregisterService(domain, stub->getStubAdapter()->getInterface(), testAddress2);
}

TEST_F(DBusMainLoopTest, ProxyAndServiceInSameDemoMainloopCanCommunicate) {
    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceStubDefault> stub = std::make_shared<
        VERSION::commonapi::tests::TestInterfaceStubDefault>();
    ASSERT_TRUE(runtime_->registerService(domain, testAddress4, stub, context_));

    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(domain, testAddress4, context_);
    ASSERT_TRUE((bool) proxy);

    while (!proxy->isAvailable()) {
        mainLoop_->doSingleIteration();
        usleep(50000);
    }

    uint32_t uint32Value = 42;
    std::string stringValue = "Hai :)";

    std::future<CommonAPI::CallStatus> futureStatus = proxy->testVoidPredefinedTypeMethodAsync(
                    uint32Value,
                    stringValue,
                    [&] (const CommonAPI::CallStatus& status) {
                        EXPECT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(status));
                        mainLoop_->stop();
                    }
    );

    mainLoop_->run();

    ASSERT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(futureStatus.get()));

    runtime_->unregisterService(domain, stub->getStubAdapter()->getInterface(), testAddress4);
}


class BigDataTestStub : public VERSION::commonapi::tests::TestInterfaceStubDefault {
    void testDerivedTypeMethod(
            ::commonapi::tests::DerivedTypeCollection::TestEnumExtended2 testEnumExtended2InValue,
            ::commonapi::tests::DerivedTypeCollection::TestMap testMapInValue,
            ::commonapi::tests::DerivedTypeCollection::TestEnumExtended2& testEnumExtended2OutValue,
            ::commonapi::tests::DerivedTypeCollection::TestMap& testMapOutValue) {
        testEnumExtended2OutValue = testEnumExtended2InValue;
        testMapOutValue = testMapInValue;
    }
};

TEST_F(DBusMainLoopTest, DemoMainloopClientsHandleNonavailableServices) {
    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(domain, testAddress3, context_);
    ASSERT_TRUE((bool) proxy);

    uint32_t uint32Value = 42;
    std::string stringValue = "Hai :)";

    std::future<CommonAPI::CallStatus> futureStatus = proxy->testVoidPredefinedTypeMethodAsync(
                    uint32Value,
                    stringValue,
                    [&] (const CommonAPI::CallStatus& status) {
                        //Will never be called, @see DBusProxyAsyncCallbackHandler
                        EXPECT_EQ(toString(CommonAPI::CallStatus::NOT_AVAILABLE), toString(status));
                    }
    );

    ASSERT_EQ(toString(CommonAPI::CallStatus::NOT_AVAILABLE), toString(futureStatus.get()));
}

//##################################################################################################
#ifndef WIN32
class GDispatchWrapper: public GSource {
 public:
    GDispatchWrapper(CommonAPI::DispatchSource* dispatchSource): dispatchSource_(dispatchSource) {}
    CommonAPI::DispatchSource* dispatchSource_;
};

gboolean dispatchPrepare(GSource* source, gint* timeout) {
    (void)timeout;
    int64_t eventTimeout;
    return static_cast<GDispatchWrapper*>(source)->dispatchSource_->prepare(eventTimeout);
}

gboolean dispatchCheck(GSource* source) {
    return static_cast<GDispatchWrapper*>(source)->dispatchSource_->check();
}

gboolean dispatchExecute(GSource* source, GSourceFunc callback, gpointer userData) {
    (void)callback;
    (void)userData;
    static_cast<GDispatchWrapper*>(source)->dispatchSource_->dispatch();
    return true;
}

gboolean gWatchDispatcher(GIOChannel *source, GIOCondition condition, gpointer userData) {
    (void)source;
    CommonAPI::Watch* watch = static_cast<CommonAPI::Watch*>(userData);
    watch->dispatch(condition);
    return true;
}

gboolean gTimeoutDispatcher(void* userData) {
    return static_cast<CommonAPI::DispatchSource*>(userData)->dispatch();
}


static GSourceFuncs standardGLibSourceCallbackFuncs = {
        dispatchPrepare,
        dispatchCheck,
        dispatchExecute,
        NULL,
        NULL,
        NULL
};


class DBusInGLibMainLoopTest: public ::testing::Test {
 protected:
    virtual void SetUp() {
        running_ = true;

        runtime_ = CommonAPI::Runtime::get();
        ASSERT_TRUE((bool) runtime_);

        context_ = std::make_shared<CommonAPI::MainLoopContext>();
        ASSERT_TRUE((bool) context_);

        doSubscriptions();
    }

    virtual void TearDown() {
    }

    void doSubscriptions() {
        context_->subscribeForTimeouts(
                std::bind(&DBusInGLibMainLoopTest::timeoutAddedCallback, this, std::placeholders::_1, std::placeholders::_2),
                std::bind(&DBusInGLibMainLoopTest::timeoutRemovedCallback, this, std::placeholders::_1)
        );

        context_->subscribeForWatches(
                std::bind(&DBusInGLibMainLoopTest::watchAddedCallback, this, std::placeholders::_1, std::placeholders::_2),
                std::bind(&DBusInGLibMainLoopTest::watchRemovedCallback, this, std::placeholders::_1)
        );

        context_->subscribeForWakeupEvents(
                std::bind(&DBusInGLibMainLoopTest::wakeupMain, this)
        );
    }


 public:
    std::shared_ptr<CommonAPI::MainLoopContext> context_;
    std::shared_ptr<CommonAPI::Runtime> runtime_;
    bool running_;
    static constexpr bool mayBlock_ = true;

    std::map<CommonAPI::DispatchSource*, GSource*> gSourceMappings;

    GIOChannel* dbusChannel_;

    void watchAddedCallback(CommonAPI::Watch* watch, const CommonAPI::DispatchPriority dispatchPriority) {
        (void)dispatchPriority;
        const pollfd& fileDesc = watch->getAssociatedFileDescriptor();
        dbusChannel_ = g_io_channel_unix_new(fileDesc.fd);

        GSource* gWatch = g_io_create_watch(dbusChannel_, static_cast<GIOCondition>(fileDesc.events));
        g_source_set_callback(gWatch, reinterpret_cast<GSourceFunc>(&gWatchDispatcher), watch, NULL);

        const auto& dependentSources = watch->getDependentDispatchSources();
        for (auto dependentSourceIterator = dependentSources.begin();
                dependentSourceIterator != dependentSources.end();
                dependentSourceIterator++) {
            GSource* gDispatchSource = g_source_new(&standardGLibSourceCallbackFuncs, sizeof(GDispatchWrapper));
            static_cast<GDispatchWrapper*>(gDispatchSource)->dispatchSource_ = *dependentSourceIterator;

            g_source_add_child_source(gWatch, gDispatchSource);

            gSourceMappings.insert( {*dependentSourceIterator, gDispatchSource} );
        }
        g_source_attach(gWatch, NULL);
    }

    void watchRemovedCallback(CommonAPI::Watch* watch) {
        g_source_remove_by_user_data(watch);

        if(dbusChannel_) {
            g_io_channel_unref(dbusChannel_);
            dbusChannel_ = NULL;
        }

        const auto& dependentSources = watch->getDependentDispatchSources();
        for (auto dependentSourceIterator = dependentSources.begin();
                dependentSourceIterator != dependentSources.end();
                dependentSourceIterator++) {
            g_source_new(&standardGLibSourceCallbackFuncs, sizeof(GDispatchWrapper));
            GSource* gSource = gSourceMappings.find(*dependentSourceIterator)->second;
            g_source_destroy(gSource);
            g_source_unref(gSource);
        }
    }

    void timeoutAddedCallback(CommonAPI::Timeout* commonApiTimeoutSource, const CommonAPI::DispatchPriority dispatchPriority) {
        (void)dispatchPriority;
        GSource* gTimeoutSource = g_timeout_source_new(guint(commonApiTimeoutSource->getTimeoutInterval()));
        g_source_set_callback(gTimeoutSource, &gTimeoutDispatcher, commonApiTimeoutSource, NULL);
        g_source_attach(gTimeoutSource, NULL);
    }

    void timeoutRemovedCallback(CommonAPI::Timeout* timeout) {
        g_source_remove_by_user_data(timeout);
    }

    void wakeupMain() {
        g_main_context_wakeup(NULL);
    }
};


TEST_F(DBusInGLibMainLoopTest, ProxyInGLibMainloopCanCallMethods) {
    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(domain, testAddress5, context_);
    ASSERT_TRUE((bool) proxy);

    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceStubDefault> stub = std::make_shared<
            VERSION::commonapi::tests::TestInterfaceStubDefault>();
    ASSERT_TRUE(runtime_->registerService(domain, testAddress5, stub, "connection"));

    while(!proxy->isAvailable()) {
        g_main_context_iteration(NULL, mayBlock_);
        usleep(50000);
    }

    uint32_t uint32Value = 24;
    std::string stringValue = "Hai :)";

    std::future<CommonAPI::CallStatus> futureStatus = proxy->testVoidPredefinedTypeMethodAsync(
                    uint32Value,
                    stringValue,
                    [&] (const CommonAPI::CallStatus& status) {
                        EXPECT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(status));
                        running_ = false;
                    }
    );

    while(running_) {
        g_main_context_iteration(NULL, mayBlock_);
        usleep(50000);
    }

    ASSERT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(futureStatus.get()));

    runtime_->unregisterService(domain, stub->getStubAdapter()->getInterface(), testAddress5);
}


TEST_F(DBusInGLibMainLoopTest, ServiceInGLibMainloopCanBeAddressed) {
    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(domain, testAddress6, "connection");
    ASSERT_TRUE((bool) proxy);

    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceStubDefault> stub = std::make_shared<
            VERSION::commonapi::tests::TestInterfaceStubDefault>();
    ASSERT_TRUE(runtime_->registerService(domain, testAddress6, stub, context_));

    uint32_t uint32Value = 42;
    std::string stringValue = "Ciao (:";

    while(!proxy->isAvailable()) {
        g_main_context_iteration(NULL, mayBlock_);
        usleep(50000);
    }

    std::future<CommonAPI::CallStatus> futureStatus = proxy->testVoidPredefinedTypeMethodAsync(
                    uint32Value,
                    stringValue,
                    [&] (const CommonAPI::CallStatus& status) {
                        EXPECT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(status));
                        running_ = false;
                        //Wakeup needed as the service will be in a poll-block when the client
                        //call returns, and no other timeout is present to get him out of there.
                        g_main_context_wakeup(NULL);
                    }
    );

    while(running_) {
        g_main_context_iteration(NULL, mayBlock_);
        usleep(50000);
    }

    ASSERT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(futureStatus.get()));

    runtime_->unregisterService(domain, stub->getStubAdapter()->getInterface(), testAddress6);
}


TEST_F(DBusInGLibMainLoopTest, ProxyAndServiceInSameGlibMainloopCanCommunicate) {
    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(domain, testAddress7, context_);
    ASSERT_TRUE((bool) proxy);

    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceStubDefault> stub = std::make_shared<
            VERSION::commonapi::tests::TestInterfaceStubDefault>();
    ASSERT_TRUE(runtime_->registerService(domain, testAddress7, stub, context_));

    uint32_t uint32Value = 42;
    std::string stringValue = "Ciao (:";

    while(!proxy->isAvailable()) {
        g_main_context_iteration(NULL, mayBlock_);
        usleep(50000);
    }

    std::future<CommonAPI::CallStatus> futureStatus = proxy->testVoidPredefinedTypeMethodAsync(
                    uint32Value,
                    stringValue,
                    [&] (const CommonAPI::CallStatus& status) {
                        EXPECT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(status));
                        running_ = false;
                        //Wakeup needed as the service will be in a poll-block when the client
                        //call returns, and no other timeout is present to get him out of there.
                        g_main_context_wakeup(NULL);
                    }
    );

    while(running_) {
        g_main_context_iteration(NULL, mayBlock_);
        usleep(50000);
    }

    ASSERT_EQ(toString(CommonAPI::CallStatus::SUCCESS), toString(futureStatus.get()));

    runtime_->unregisterService(domain, stub->getStubAdapter()->getInterface(), testAddress7);
}
#endif

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
