// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/DBus/DBusProxyAsyncCallbackHandler.hpp>

#include <gtest/gtest.h>
#include <dbus/dbus.h>

#include <chrono>
#include <cstring>
#include <thread>

bool replyArrived;

class LibdbusTest: public ::testing::Test {
 protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

class DBusConnectionTest: public ::testing::Test {
protected:
    virtual void SetUp() {
        dbusConnection_ = CommonAPI::DBus::DBusConnection::getBus(CommonAPI::DBus::DBusType_t::SESSION, "connection1");
    }

    virtual void TearDown() {
    }

    std::shared_ptr<CommonAPI::DBus::DBusConnection> dbusConnection_;
};

TEST_F(DBusConnectionTest, IsInitiallyDisconnected) {
    ASSERT_FALSE(dbusConnection_->isConnected());
}

TEST_F(DBusConnectionTest, ConnectAndDisconnectWork) {
    ASSERT_TRUE(dbusConnection_->connect());
    ASSERT_TRUE(dbusConnection_->isConnected());

    dbusConnection_->disconnect();
    ASSERT_FALSE(dbusConnection_->isConnected());
}

TEST_F(DBusConnectionTest, ConnectionStatusEventWorks) {
    uint32_t connectionStatusEventCount = 0;
    CommonAPI::AvailabilityStatus connectionStatus = CommonAPI::AvailabilityStatus::UNKNOWN;

    auto connectionStatusSubscription = dbusConnection_->getConnectionStatusEvent().subscribe(std::bind(
                    [&connectionStatusEventCount, &connectionStatus](CommonAPI::AvailabilityStatus availabilityStatus) {
                        ++connectionStatusEventCount;
                        connectionStatus = availabilityStatus;
                    },
                    std::placeholders::_1));

    ASSERT_FALSE(dbusConnection_->isConnected());
    ASSERT_EQ(connectionStatusEventCount, 0u);

    uint32_t expectedEventCount = 0;
    while (expectedEventCount < 10) {
        ASSERT_TRUE(dbusConnection_->connect());
        ASSERT_TRUE(dbusConnection_->isConnected());
        std::this_thread::sleep_for(std::chrono::microseconds(40000));
        ASSERT_EQ(connectionStatusEventCount, ++expectedEventCount);
        ASSERT_EQ(connectionStatus, CommonAPI::AvailabilityStatus::AVAILABLE);

        dbusConnection_->disconnect();
        ASSERT_FALSE(dbusConnection_->isConnected());
        std::this_thread::sleep_for(std::chrono::microseconds(40000));
        ASSERT_EQ(connectionStatusEventCount, ++expectedEventCount);
        ASSERT_EQ(connectionStatus, CommonAPI::AvailabilityStatus::NOT_AVAILABLE);
    }

    dbusConnection_->getConnectionStatusEvent().unsubscribe(connectionStatusSubscription);
    ASSERT_EQ(connectionStatusEventCount, expectedEventCount);

    ASSERT_TRUE(dbusConnection_->connect());
    ASSERT_TRUE(dbusConnection_->isConnected());
    ASSERT_EQ(connectionStatusEventCount, expectedEventCount);

    dbusConnection_->disconnect();
    ASSERT_FALSE(dbusConnection_->isConnected());
    ASSERT_EQ(connectionStatusEventCount, expectedEventCount);
}

TEST_F(DBusConnectionTest, SendingAsyncDBusMessagesWorks) {
    const char service[] = "commonapi.dbus.test.TestInterface_commonapi.dbus.test.TestObject";
    const char objectPath[] = "/commonapi/dbus/test/TestObject";
    const char interfaceName[] = "commonapi.dbus.test.TestInterface";
    const char methodName[] = "TestMethod";

    auto interfaceHandlerDBusConnection = CommonAPI::DBus::DBusConnection::getBus(CommonAPI::DBus::DBusType_t::SESSION, "connection2");

    ASSERT_TRUE(interfaceHandlerDBusConnection->connect());
    ASSERT_TRUE(interfaceHandlerDBusConnection->requestServiceNameAndBlock(service));

    uint32_t serviceHandlerDBusMessageCount = 0;
    uint32_t clientReplyHandlerDBusMessageCount = 0;

    interfaceHandlerDBusConnection->setObjectPathMessageHandler(
                    [&serviceHandlerDBusMessageCount, &interfaceHandlerDBusConnection] (CommonAPI::DBus::DBusMessage dbusMessage) -> bool {
                        ++serviceHandlerDBusMessageCount;
                        CommonAPI::DBus::DBusMessage dbusMessageReply = dbusMessage.createMethodReturn("");
                        interfaceHandlerDBusConnection->sendDBusMessage(dbusMessageReply);
                        return true;
                    }
                    );

    interfaceHandlerDBusConnection->registerObjectPath(objectPath);

    ASSERT_TRUE(dbusConnection_->connect());

    for (uint32_t expectedDBusMessageCount = 1; expectedDBusMessageCount <= 10; expectedDBusMessageCount++) {
        CommonAPI::DBus::DBusMessage dbusMessageCall = CommonAPI::DBus::DBusMessage::createMethodCall(
                        CommonAPI::DBus::DBusAddress(service, objectPath, interfaceName),
                        methodName,
                        "");

        CommonAPI::DBus::DBusOutputStream dbusOutputStream(dbusMessageCall);

        auto func = [&clientReplyHandlerDBusMessageCount](CommonAPI::CallStatus status) {
            ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, status);
            ++clientReplyHandlerDBusMessageCount;
        };

        CommonAPI::DBus::DBusProxyAsyncCallbackHandler<CommonAPI::DBus::DBusConnection>::Delegate
            delegate(dbusConnection_->shared_from_this(), func);

        dbusConnection_->sendDBusMessageWithReplyAsync(
                        dbusMessageCall,
                        CommonAPI::DBus::DBusProxyAsyncCallbackHandler<CommonAPI::DBus::DBusConnection>::create(delegate, std::tuple<>()),
                        &CommonAPI::DBus::defaultCallInfo);

        for (int i = 0; i < 100; i++) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
        }

        ASSERT_EQ(serviceHandlerDBusMessageCount, expectedDBusMessageCount);

        ASSERT_EQ(clientReplyHandlerDBusMessageCount, expectedDBusMessageCount);
    }

    dbusConnection_->disconnect();

    interfaceHandlerDBusConnection->unregisterObjectPath(objectPath);

    ASSERT_TRUE(interfaceHandlerDBusConnection->releaseServiceName(service));
    interfaceHandlerDBusConnection->disconnect();
}

/*TEST_F(DBusConnectionTest, SendingAsyncDBusMessagesWorksManualDispatch) {
    const char service[] = "commonapi.dbus.test.TestInterface_commonapi.dbus.test.TestObject";
    const char objectPath[] = "/commonapi/dbus/test/TestObject";
    const char interfaceName[] = "commonapi.dbus.test.TestInterface";
    const char methodName[] = "TestMethod";

    auto interfaceHandlerDBusConnection = CommonAPI::DBus::DBusConnection::getBus(CommonAPI::DBus::DBusType_t::SESSION);

    ASSERT_TRUE(interfaceHandlerDBusConnection->connect(false));
    ASSERT_TRUE(interfaceHandlerDBusConnection->requestServiceNameAndBlock(service));

    uint32_t serviceHandlerDBusMessageCount = 0;
    uint32_t clientReplyHandlerDBusMessageCount = 0;

    interfaceHandlerDBusConnection->setObjectPathMessageHandler(
                    [&serviceHandlerDBusMessageCount, &interfaceHandlerDBusConnection] (CommonAPI::DBus::DBusMessage dbusMessage) -> bool {
                        ++serviceHandlerDBusMessageCount;
                        CommonAPI::DBus::DBusMessage dbusMessageReply = dbusMessage.createMethodReturn("");
                        interfaceHandlerDBusConnection->sendDBusMessage(dbusMessageReply);
                        return true;
                    }
                    );

    interfaceHandlerDBusConnection->registerObjectPath(objectPath);

    ASSERT_TRUE(dbusConnection_->connect(false));

    for (uint32_t expectedDBusMessageCount = 1; expectedDBusMessageCount <= 10; expectedDBusMessageCount++) {
        CommonAPI::DBus::DBusMessage dbusMessageCall = CommonAPI::DBus::DBusMessage::createMethodCall(
                        CommonAPI::DBus::DBusAddress(service, objectPath, interfaceName),
                        methodName,
                        "");

        CommonAPI::DBus::DBusOutputStream dbusOutputStream(dbusMessageCall);

        dbusConnection_->sendDBusMessageWithReplyAsync(
                        dbusMessageCall,
                        CommonAPI::DBus::DBusProxyAsyncCallbackHandler<>::create(
                                        [&clientReplyHandlerDBusMessageCount](CommonAPI::CallStatus status) {
                                            ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, status);
                                            ++clientReplyHandlerDBusMessageCount;
                                        }, std::tuple<>()),
                                        &CommonAPI::DBus::defaultCallInfo);

        for (int i = 0; i < 10 && serviceHandlerDBusMessageCount < expectedDBusMessageCount; i++) {
            interfaceHandlerDBusConnection->readWriteDispatch(100);
        }

        ASSERT_EQ(serviceHandlerDBusMessageCount, expectedDBusMessageCount);

        for (int i = 0; i < 10 && clientReplyHandlerDBusMessageCount < expectedDBusMessageCount; i++) {
            dbusConnection_->readWriteDispatch(100);
        }

        ASSERT_EQ(clientReplyHandlerDBusMessageCount, expectedDBusMessageCount);
    }

    dbusConnection_->disconnect();

    interfaceHandlerDBusConnection->unregisterObjectPath(objectPath);

    ASSERT_TRUE(interfaceHandlerDBusConnection->releaseServiceName(service));
    interfaceHandlerDBusConnection->disconnect();
}*/

std::mutex dispatchMutex;
std::condition_variable dispatchCondition;
bool dispatchReady = false;

void dispatch(::DBusConnection* libdbusConnection) {
    dbus_bool_t success = TRUE;
    std::unique_lock<std::mutex> lock(dispatchMutex);
    dispatchReady = true;
    dispatchCondition.wait(lock);
    while (success) {
        success = dbus_connection_read_write_dispatch(libdbusConnection, 1);
    }
}

bool suicide = false;

void notifyThunk(DBusPendingCall*, void* data) {
    ::DBusConnection* libdbusConnection = reinterpret_cast<DBusConnection*>(data);
    dbus_connection_close(libdbusConnection);
    dbus_connection_unref(libdbusConnection);
    suicide = true;
}

TEST_F(DBusConnectionTest, LibdbusConnectionsMayCommitSuicide) {
    dispatchReady = false;
    suicide = false;

    const ::DBusBusType libdbusType = ::DBusBusType::DBUS_BUS_SESSION;
    ::DBusError libdbusError;
    dbus_error_init(&libdbusError);
    ::DBusConnection* libdbusConnection = dbus_bus_get_private(libdbusType, &libdbusError);

    assert(libdbusConnection);
    dbus_connection_set_exit_on_disconnect(libdbusConnection, false);

    auto dispatchThread = std::thread(&dispatch, libdbusConnection);

    {
        std::unique_lock<std::mutex> dispatchLock(dispatchMutex);
        while(!dispatchReady) {
            dispatchLock.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds((100000 * 5)));
            dispatchLock.lock();
        }
    }

    ::DBusMessage* libdbusMessageCall = dbus_message_new_method_call(
                    "org.freedesktop.DBus",
                    "/org/freedesktop/DBus",
                    "org.freedesktop.DBus",
                    "ListNames");

    dbus_message_set_signature(libdbusMessageCall, "");

    DBusPendingCall* libdbusPendingCall;

    dbus_connection_send_with_reply(
                    libdbusConnection,
                    libdbusMessageCall,
                    &libdbusPendingCall,
                    500);

    dbus_pending_call_set_notify(
                    libdbusPendingCall,
                    notifyThunk,
                    libdbusConnection,
                    NULL);

    dispatchCondition.notify_one();

    dispatchThread.join();
    ASSERT_TRUE(suicide);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
