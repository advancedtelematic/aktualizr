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

#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>

#include "commonapi/tests/PredefinedTypeCollection.hpp"
#include "commonapi/tests/DerivedTypeCollection.hpp"

#include "v1/commonapi/tests/TestInterfaceManagerProxy.hpp"
#include "v1/commonapi/tests/TestInterfaceManagerStubDefault.hpp"
#include "v1/commonapi/tests/TestInterfaceManagerDBusProxy.hpp"
#include "v1/commonapi/tests/TestInterfaceManagerDBusStubAdapter.hpp"

#include "v1/commonapi/tests/TestInterfaceProxy.hpp"
#include "v1/commonapi/tests/TestInterfaceStubDefault.hpp"
#include "v1/commonapi/tests/TestInterfaceDBusStubAdapter.hpp"
#include "v1/commonapi/tests/TestInterfaceDBusProxy.hpp"

#define VERSION v1_0

class SelectiveBroadcastSender: public VERSION::commonapi::tests::TestInterfaceStubDefault {
public:

    SelectiveBroadcastSender():
        acceptSubs(true),
        sentBroadcasts(0) {

    }

    virtual ~SelectiveBroadcastSender() {

    }

    void startSending() {
        sentBroadcasts = 0;
        selectiveBroadcastSender = std::thread(&SelectiveBroadcastSender::send, this);
        selectiveBroadcastSender.detach();
    }

    void send() {
        sentBroadcasts++;
        fireTestSelectiveBroadcastSelective();
    }

    void onTestSelectiveBroadcastSelectiveSubscriptionChanged(
            const std::shared_ptr<CommonAPI::ClientId> clientId,
            const CommonAPI::SelectiveBroadcastSubscriptionEvent event) {

        if (event == CommonAPI::SelectiveBroadcastSubscriptionEvent::SUBSCRIBED)
            lastSubscribedClient = clientId;
    }

    bool onTestSelectiveBroadcastSelectiveSubscriptionRequested(
                    const std::shared_ptr<CommonAPI::ClientId> clientId) {
        (void)clientId;
        return acceptSubs;
    }

    void sendToLastSubscribedClient()
    {
        sentBroadcasts++;
        std::shared_ptr<CommonAPI::ClientIdList> receivers = std::make_shared<CommonAPI::ClientIdList>();
        receivers->insert(lastSubscribedClient);

        fireTestSelectiveBroadcastSelective(receivers);
    }


    int getNumberOfSubscribedClients() {
        return static_cast<int>(getSubscribersForTestSelectiveBroadcastSelective()->size());

    }

    bool acceptSubs;

private:
    std::thread selectiveBroadcastSender;
    int sentBroadcasts;

    std::shared_ptr<CommonAPI::ClientId> lastSubscribedClient;
};

class DBusBroadcastTest: public ::testing::Test {
protected:
   virtual void SetUp() {
       runtime_ = CommonAPI::Runtime::get();
       ASSERT_TRUE((bool)runtime_);

       serviceAddressObject_ = CommonAPI::Address(serviceAddress_);

       selectiveBroadcastArrivedAtProxyFromSameConnection1 = 0;
       selectiveBroadcastArrivedAtProxyFromSameConnection2 = 0;
       selectiveBroadcastArrivedAtProxyFromOtherConnection = 0;
   }

   virtual void TearDown() {
       runtime_->unregisterService(serviceAddressObject_.getDomain(), serviceAddressInterface_, serviceAddressObject_.getInstance());
   }

   std::shared_ptr<CommonAPI::Runtime> runtime_;

   static const std::string serviceAddress_;
   static const std::string managerServiceAddress_;
   CommonAPI::Address serviceAddressObject_;
   std::string serviceAddressInterface_;
   static const CommonAPI::ConnectionId_t connectionIdService_;
   static const CommonAPI::ConnectionId_t connectionIdClient1_;
   static const CommonAPI::ConnectionId_t connectionIdClient2_;
   static const CommonAPI::ConnectionId_t connectionIdClient3;

   int selectiveBroadcastArrivedAtProxyFromSameConnection1;
   int selectiveBroadcastArrivedAtProxyFromSameConnection2;
   int selectiveBroadcastArrivedAtProxyFromOtherConnection;

public:
   void selectiveBroadcastCallbackForProxyFromSameConnection1() {
       selectiveBroadcastArrivedAtProxyFromSameConnection1++;
   }

   void selectiveBroadcastCallbackForProxyFromSameConnection2() {
       selectiveBroadcastArrivedAtProxyFromSameConnection2++;
   }

   void selectiveBroadcastCallbackForProxyFromOtherConnection() {
       selectiveBroadcastArrivedAtProxyFromOtherConnection++;
   }
};

const std::string DBusBroadcastTest::serviceAddress_ = "local:CommonAPI.DBus.tests.TestInterface:CommonAPI.DBus.tests.TestInterfaceManager.TestService";
const std::string DBusBroadcastTest::managerServiceAddress_ = "local:CommonAPI.DBus.tests.TestInterfaceManager:CommonAPI.DBus.tests.TestInterfaceManager";
const CommonAPI::ConnectionId_t DBusBroadcastTest::connectionIdService_ = "service";
const CommonAPI::ConnectionId_t DBusBroadcastTest::connectionIdClient1_ = "client1";
const CommonAPI::ConnectionId_t DBusBroadcastTest::connectionIdClient2_ = "client2";
const CommonAPI::ConnectionId_t DBusBroadcastTest::connectionIdClient3 = "client3";

TEST_F(DBusBroadcastTest, ProxysCanHandleBroadcast) {
    auto stub = std::make_shared<SelectiveBroadcastSender>();
    serviceAddressInterface_ = stub->getStubAdapter()->getInterface();

    bool serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
    for (unsigned int i = 0; !serviceRegistered && i < 100; ++i) {
        serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
        usleep(10000);
    }
    ASSERT_TRUE(serviceRegistered);



    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance());

    VERSION::commonapi::tests::TestInterfaceProxyDefault::TestPredefinedTypeBroadcastEvent& broadcastEvent =
                    proxy->getTestPredefinedTypeBroadcastEvent();

    bool callbackArrived = false;

    broadcastEvent.subscribe([&](uint32_t intParam, std::string stringParam) {
            (void)intParam;
            (void)stringParam;
            callbackArrived = true;
    });

    stub->fireTestPredefinedTypeBroadcastEvent(2, "xyz");

    for(unsigned int i=0; i<100 && !callbackArrived; i++) {
        usleep(10000);
    }

    ASSERT_TRUE(callbackArrived);
}

TEST_F(DBusBroadcastTest, ProxysCanUnsubscribeFromBroadcastAndSubscribeAgain) {
    auto stub = std::make_shared<SelectiveBroadcastSender>();
    serviceAddressInterface_ = stub->getStubAdapter()->getInterface();

    bool serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
    for (unsigned int i = 0; !serviceRegistered && i < 100; ++i) {
        serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
        usleep(10000);
    }
    ASSERT_TRUE(serviceRegistered);



    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance());

    VERSION::commonapi::tests::TestInterfaceProxyDefault::TestPredefinedTypeBroadcastEvent& broadcastEvent =
                    proxy->getTestPredefinedTypeBroadcastEvent();

    bool callbackArrived = false;

    auto broadcastSubscription = broadcastEvent.subscribe([&](uint32_t intParam, std::string stringParam) {
            (void)stringParam;
            EXPECT_EQ(intParam, 1);
            callbackArrived = true;
    });

    stub->fireTestPredefinedTypeBroadcastEvent(1, "xyz");

    for(unsigned int i=0; i<100 && !callbackArrived; i++) {
        usleep(10000);
    }

    ASSERT_TRUE(callbackArrived);

    broadcastEvent.unsubscribe(broadcastSubscription);

    callbackArrived = false;

    auto broadcastSubscription2 = broadcastEvent.subscribe([&](uint32_t intParam, std::string stringParam) {
        (void)stringParam;
        EXPECT_EQ(intParam, 2);
        callbackArrived = true;
    });

    stub->fireTestPredefinedTypeBroadcastEvent(2, "xyz");

    for(unsigned int i=0; i<100 && !callbackArrived; i++) {
        usleep(10000);
    }

    ASSERT_TRUE(callbackArrived);

    broadcastEvent.unsubscribe(broadcastSubscription2);
}

TEST_F(DBusBroadcastTest, ProxysCanUnsubscribeFromBroadcastAndSubscribeAgainInALoop) {
    auto stub = std::make_shared<SelectiveBroadcastSender>();
    serviceAddressInterface_ = stub->getStubAdapter()->getInterface();

    bool serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
    for (unsigned int i = 0; !serviceRegistered && i < 100; ++i) {
        serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
        usleep(10000);
    }
    ASSERT_TRUE(serviceRegistered);


    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance());

    VERSION::commonapi::tests::TestInterfaceProxyDefault::TestPredefinedTypeBroadcastEvent& broadcastEvent =
                    proxy->getTestPredefinedTypeBroadcastEvent();

    for(unsigned int i=0; i<10; i++) {
        bool callbackArrived = false;

        auto broadcastSubscription = broadcastEvent.subscribe([&,i](uint32_t intParam, std::string stringParam) {
            (void)stringParam;
            EXPECT_EQ(intParam, i);
            callbackArrived = true;
        });

        stub->fireTestPredefinedTypeBroadcastEvent(i, "xyz");

        for(unsigned int j=0; j<100 && !callbackArrived; j++) {
            usleep(10000);
        }

        ASSERT_TRUE(callbackArrived);

        broadcastEvent.unsubscribe(broadcastSubscription);
    }
}

TEST_F(DBusBroadcastTest, ProxysCanUnsubscribeFromBroadcastAndSubscribeAgainWithOtherProxy) {
    auto stub = std::make_shared<SelectiveBroadcastSender>();
    serviceAddressInterface_ = stub->getStubAdapter()->getInterface();

    bool serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
    for (unsigned int i = 0; !serviceRegistered && i < 100; ++i) {
        serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
        usleep(10000);
    }
    ASSERT_TRUE(serviceRegistered);


    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance());

    VERSION::commonapi::tests::TestInterfaceProxyDefault::TestPredefinedTypeBroadcastEvent& broadcastEvent =
                    proxy->getTestPredefinedTypeBroadcastEvent();

    bool callbackArrived = false;

    auto broadcastSubscription = broadcastEvent.subscribe([&](uint32_t intParam, std::string stringParam) {
        (void)stringParam;
        EXPECT_EQ(intParam, 1);
        callbackArrived = true;
    });

    stub->fireTestPredefinedTypeBroadcastEvent(1, "xyz");

    for(unsigned int i=0; i<100 && !callbackArrived; i++) {
        usleep(10000);
    }

    ASSERT_TRUE(callbackArrived);

    broadcastEvent.unsubscribe(broadcastSubscription);

    auto proxy2 = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance());

    VERSION::commonapi::tests::TestInterfaceProxyDefault::TestPredefinedTypeBroadcastEvent& broadcastEvent2 =
                    proxy->getTestPredefinedTypeBroadcastEvent();

    callbackArrived = false;

    auto broadcastSubscription2 = broadcastEvent2.subscribe([&](uint32_t intParam, std::string stringParam) {
        (void)stringParam;
        EXPECT_EQ(intParam, 2);
        callbackArrived = true;
    });

    stub->fireTestPredefinedTypeBroadcastEvent(2, "xyz");

    for(unsigned int i=0; i<100 && !callbackArrived; i++) {
        usleep(10000);
    }

    ASSERT_TRUE(callbackArrived);

    broadcastEvent.unsubscribe(broadcastSubscription2);
}

TEST_F(DBusBroadcastTest, ProxysCanUnsubscribeFromBroadcastAndSubscribeAgainWhileOtherProxyIsStillSubscribed) {
    // register service
    auto stub = std::make_shared<SelectiveBroadcastSender>();
    serviceAddressInterface_ = stub->getStubAdapter()->getInterface();

    bool serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
    for (unsigned int i = 0; !serviceRegistered && i < 100; ++i) {
        serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
        usleep(10000);
    }
    ASSERT_TRUE(serviceRegistered);

    // build 2 proxies
    auto proxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance());
    auto proxy2 = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance());

    VERSION::commonapi::tests::TestInterfaceProxyDefault::TestPredefinedTypeBroadcastEvent& broadcastEvent =
                    proxy->getTestPredefinedTypeBroadcastEvent();

    VERSION::commonapi::tests::TestInterfaceProxyDefault::TestPredefinedTypeBroadcastEvent& broadcastEvent2 =
                    proxy2->getTestPredefinedTypeBroadcastEvent();

    bool callback1Arrived = false;
    bool callback2Arrived = false;

    // subscribe for each proxy's broadcast event
    auto broadcastSubscription = broadcastEvent.subscribe([&](uint32_t intParam, std::string stringParam) {
        (void)intParam;
        (void)stringParam;
        callback1Arrived = true;
    });

    auto broadcastSubscription2 = broadcastEvent2.subscribe([&](uint32_t intParam, std::string stringParam) {
        (void)intParam;
        (void)stringParam;
        callback2Arrived = true;
    });

    // fire broadcast and wait for results
    stub->fireTestPredefinedTypeBroadcastEvent(1, "xyz");

    for(unsigned int i=0; i<100 && !(callback1Arrived && callback2Arrived); i++) {
        usleep(10000);
    }

    const bool callbackOnBothSubscriptionsArrived = callback1Arrived && callback2Arrived;

    EXPECT_TRUE(callbackOnBothSubscriptionsArrived);

    // unsubscribe from one proxy's broadcast
    broadcastEvent.unsubscribe(broadcastSubscription);

    // fire broadcast again
    callback1Arrived = false;
    callback2Arrived = false;

    stub->fireTestPredefinedTypeBroadcastEvent(2, "xyz");

    for(unsigned int i=0; i<100; i++) {
        usleep(10000);
    }

    const bool onlyCallback2Arrived = !callback1Arrived && callback2Arrived;

    EXPECT_TRUE(onlyCallback2Arrived);

    // subscribe first proxy again
    broadcastSubscription = broadcastEvent.subscribe([&](uint32_t intParam, std::string stringParam) {
        (void)intParam;
        (void)stringParam;
        callback1Arrived = true;
    });

    // fire broadcast another time
    callback1Arrived = false;
    callback2Arrived = false;

    stub->fireTestPredefinedTypeBroadcastEvent(1, "xyz");

    for(unsigned int i=0; i<100 && !(callback1Arrived && callback2Arrived); i++) {
        usleep(10000);
    }

    const bool callbackOnBothSubscriptionsArrivedAgain = callback1Arrived && callback2Arrived;

    EXPECT_TRUE(callbackOnBothSubscriptionsArrivedAgain);

    broadcastEvent.unsubscribe(broadcastSubscription);
    broadcastEvent2.unsubscribe(broadcastSubscription2);
}

TEST_F(DBusBroadcastTest, ProxysCanSubscribeForSelectiveBroadcast)
{
    auto proxyFromSameConnection = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), connectionIdClient1_);
    ASSERT_TRUE((bool)proxyFromSameConnection);
    auto proxyFromSameConnection2 = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), connectionIdClient1_);
    ASSERT_TRUE((bool)proxyFromSameConnection2);
    auto proxyFromOtherConnection = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), connectionIdClient2_);
    ASSERT_TRUE((bool)proxyFromOtherConnection);

    auto stub = std::make_shared<SelectiveBroadcastSender>();
    serviceAddressInterface_ = stub->getStubAdapter()->getInterface();

    bool serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
    for (unsigned int i = 0; !serviceRegistered && i < 100; ++i) {
        serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
        usleep(10000);
    }
    ASSERT_TRUE(serviceRegistered);

    for (unsigned int i = 0; !proxyFromSameConnection->isAvailable() && i < 200; ++i) {
        usleep(10000);
    }
    ASSERT_TRUE(proxyFromSameConnection->isAvailable());

    auto subscriptionResult1 = proxyFromSameConnection->getTestSelectiveBroadcastSelectiveEvent().subscribe(
                    std::bind(&DBusBroadcastTest::selectiveBroadcastCallbackForProxyFromSameConnection1, this));

    usleep(20000);
    ASSERT_EQ(stub->getNumberOfSubscribedClients(), 1);

    stub->send();

    usleep(200000);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromSameConnection1, 1);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromSameConnection2, 0);


    auto subscriptionResult2 = proxyFromSameConnection2->getTestSelectiveBroadcastSelectiveEvent().subscribe(std::bind(&DBusBroadcastTest::selectiveBroadcastCallbackForProxyFromSameConnection2, this));
    ASSERT_EQ(stub->getNumberOfSubscribedClients(), 1); // should still be one because they using the same connection

    stub->send();
    usleep(200000);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromSameConnection1, 2);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromSameConnection2, 1);

    proxyFromOtherConnection->getTestSelectiveBroadcastSelectiveEvent().subscribe(std::bind(&DBusBroadcastTest::selectiveBroadcastCallbackForProxyFromOtherConnection, this));
    ASSERT_EQ(stub->getNumberOfSubscribedClients(), 2); // should still be two because proxyFromSameConnection1_ is still subscribed

    proxyFromSameConnection2->getTestSelectiveBroadcastSelectiveEvent().unsubscribe(subscriptionResult2);
    ASSERT_EQ(stub->getNumberOfSubscribedClients(), 2); // should still be two because proxyFromSameConnection1_ is still subscribed

    stub->send();
    usleep(200000);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromSameConnection1, 3);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromSameConnection2, 1);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromOtherConnection, 1);

    // now only the last subscribed client (which is the one from the other connection) should receive the signal
    stub->sendToLastSubscribedClient();
    usleep(200000);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromSameConnection1, 3);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromSameConnection2, 1);
    EXPECT_EQ(selectiveBroadcastArrivedAtProxyFromOtherConnection, 2);

    proxyFromSameConnection->getTestSelectiveBroadcastSelectiveEvent().unsubscribe(subscriptionResult1);
    EXPECT_EQ(stub->getNumberOfSubscribedClients(), 1);
}

TEST_F(DBusBroadcastTest, ProxysCanBeRejectedForSelectiveBroadcast) {
    auto proxyFromSameConnection1 = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), connectionIdClient1_);
    ASSERT_TRUE((bool)proxyFromSameConnection1);
    auto proxyFromOtherConnection = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), connectionIdClient2_);
    ASSERT_TRUE((bool)proxyFromOtherConnection);


    auto stub = std::make_shared<SelectiveBroadcastSender>();
    serviceAddressInterface_ = stub->getStubAdapter()->getInterface();

    bool serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
    for (unsigned int i = 0; !serviceRegistered && i < 100; ++i) {
        serviceRegistered = runtime_->registerService(serviceAddressObject_.getDomain(), serviceAddressObject_.getInstance(), stub, connectionIdService_);
        usleep(10000);
    }
    ASSERT_TRUE(serviceRegistered);

    for (unsigned int i = 0; !proxyFromSameConnection1->isAvailable() && i < 200; ++i) {
        usleep(10000);
    }
    ASSERT_TRUE(proxyFromSameConnection1->isAvailable());

    //bool subbed = false;

    proxyFromSameConnection1->getTestSelectiveBroadcastSelectiveEvent().subscribe(
                    std::bind(&DBusBroadcastTest::selectiveBroadcastCallbackForProxyFromSameConnection1, this));
    ASSERT_EQ(stub->getNumberOfSubscribedClients(), 1);
    //ASSERT_TRUE(subbed);

    stub->acceptSubs = false;

    proxyFromOtherConnection->getTestSelectiveBroadcastSelectiveEvent().subscribe(
                    std::bind(&DBusBroadcastTest::selectiveBroadcastCallbackForProxyFromOtherConnection, this));
    ASSERT_EQ(stub->getNumberOfSubscribedClients(), 1);
    //ASSERT_FALSE(subbed);

    stub->send();

    usleep(20000);
    ASSERT_EQ(selectiveBroadcastArrivedAtProxyFromSameConnection1, 1);
    ASSERT_EQ(selectiveBroadcastArrivedAtProxyFromOtherConnection, 0);
}

TEST_F(DBusBroadcastTest, ProxyCanBeDeletedAndBuildFromNewInManagedContext) {
    int intParamValue = 0;
    bool callbackArrived = false;
    bool proxyDeleted = false;

    CommonAPI::Address managerServiceAddressObject = CommonAPI::Address(managerServiceAddress_);

    //build proxy of test interface manager
    auto managerProxy = runtime_->buildProxy<VERSION::commonapi::tests::TestInterfaceManagerProxy>(managerServiceAddressObject.getDomain(), managerServiceAddressObject.getInstance(), connectionIdClient1_);
    ASSERT_TRUE((bool)managerProxy);

    //build stub of test interface manager
    auto managerStub = std::make_shared<VERSION::commonapi::tests::TestInterfaceManagerStubDefault>();
    const std::string managerServiceInterface = managerStub->getStubAdapter()->getInterface();

    //register test interface manager
    bool managerServiceRegistered = runtime_->registerService(managerServiceAddressObject.getDomain(), managerServiceAddressObject.getInstance(), managerStub, connectionIdService_);
    for (unsigned int i = 0; !managerServiceRegistered && i < 100; ++i) {
        managerServiceRegistered = runtime_->registerService(managerServiceAddressObject.getDomain(), managerServiceAddressObject.getInstance(), managerStub, connectionIdService_);
        usleep(10000);
    }
    ASSERT_TRUE(managerServiceRegistered);

    for (unsigned int i = 0; !managerProxy->isAvailable() && i < 200; ++i) {
        usleep(10000);
    }
    ASSERT_TRUE(managerProxy->isAvailable());

    //stub and proxy of test interface
    auto stubTestInterface = std::make_shared<SelectiveBroadcastSender>();
    std::shared_ptr<VERSION::commonapi::tests::TestInterfaceProxy<>> proxyTestInterface;

    //subscribe for instance availability changed event
    CommonAPI::ProxyManager& testInterfaceProxyManager = managerProxy->getProxyManagerTestInterface();
    testInterfaceProxyManager.getInstanceAvailabilityStatusChangedEvent().subscribe
            ([&](const std::string instanceName, CommonAPI::AvailabilityStatus availabilityStatus) {
    (void)instanceName;

        if(availabilityStatus == CommonAPI::AvailabilityStatus::AVAILABLE) {
            //Create proxy for managed test interface
            proxyTestInterface = testInterfaceProxyManager.buildProxy<VERSION::commonapi::tests::TestInterfaceProxy>(serviceAddressObject_.getInstance(), connectionIdClient3);
            ASSERT_TRUE((bool)proxyTestInterface);

            for (unsigned int i = 0; !proxyTestInterface->isAvailable() && i < 200; ++i) {
                usleep(10000);
            }
            ASSERT_TRUE(proxyTestInterface->isAvailable());

            VERSION::commonapi::tests::TestInterfaceProxyDefault::TestPredefinedTypeBroadcastEvent& broadcastEvent =
                            proxyTestInterface->getTestPredefinedTypeBroadcastEvent();

            //subscribe for broadcast event
            broadcastEvent.subscribe([&](uint32_t intParam, std::string stringParam) {
                (void)stringParam;
                EXPECT_EQ(intParam, intParamValue);
                callbackArrived = true;
            });

            stubTestInterface->fireTestPredefinedTypeBroadcastEvent(intParamValue, "xyz");

        } else if(availabilityStatus == CommonAPI::AvailabilityStatus::NOT_AVAILABLE) {
            //delete proxy
            proxyTestInterface = nullptr;
            proxyDeleted = true;
        }
    });

    //register managed test interface
    intParamValue++;
    bool managedServiceRegistered = managerStub->registerManagedStubTestInterface(stubTestInterface, serviceAddressObject_.getInstance());
    ASSERT_TRUE(managedServiceRegistered);

    for(unsigned int i=0; i<200 && !callbackArrived; i++) {
         usleep(10000);
    }
    ASSERT_TRUE(callbackArrived);

    //deregister managed test interface
    managerStub->deregisterManagedStubTestInterface(serviceAddressObject_.getInstance());

    for(unsigned int i=0; i<200 && !proxyDeleted; i++) {
        usleep(10000);
    }
    ASSERT_TRUE(proxyDeleted);

    //register managed test interface again
    intParamValue++;
    callbackArrived = false;

    managedServiceRegistered = managerStub->registerManagedStubTestInterface(stubTestInterface, serviceAddressObject_.getInstance());
    ASSERT_TRUE(managedServiceRegistered);

    for(unsigned int i=0; i<200 && !callbackArrived; i++) {
         usleep(10000);
    }
    ASSERT_TRUE(callbackArrived);

    managerStub->deregisterManagedStubTestInterface(serviceAddressObject_.getInstance());
    runtime_->unregisterService(managerServiceAddressObject.getDomain(), managerServiceInterface, managerServiceAddressObject.getInstance());
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
