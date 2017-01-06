// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUS_CONNECTION_HPP_
#define COMMONAPI_DBUS_DBUS_CONNECTION_HPP_

#include <atomic>
#include <mutex>

#include <dbus/dbus.h>

#include <CommonAPI/DBus/DBusConfig.hpp>
#include <CommonAPI/DBus/DBusDaemonProxy.hpp>
#include <CommonAPI/DBus/DBusMainLoop.hpp>
#include <CommonAPI/DBus/DBusMainLoopContext.hpp>
#include <CommonAPI/DBus/DBusObjectManager.hpp>
#include <CommonAPI/DBus/DBusProxyConnection.hpp>
#include <CommonAPI/DBus/DBusServiceRegistry.hpp>

namespace CommonAPI {
namespace DBus {

class DBusMainLoop;
class DBusObjectManager;

class DBusConnectionStatusEvent
        : public DBusProxyConnection::ConnectionStatusEvent {
public:
    DBusConnectionStatusEvent(DBusConnection* dbusConnection);
    virtual ~DBusConnectionStatusEvent() {}

 protected:
    virtual void onListenerAdded(const Listener& listener, const Subscription subscription);

    // TODO: change to std::weak_ptr<DBusConnection> connection_;
    DBusConnection* dbusConnection_;

friend class DBusConnection;
};

struct WatchContext {
    WatchContext(std::weak_ptr<MainLoopContext> mainLoopContext, DispatchSource* dispatchSource) :
            mainLoopContext_(mainLoopContext), dispatchSource_(dispatchSource) {
    }

    std::weak_ptr<MainLoopContext> mainLoopContext_;
    DispatchSource* dispatchSource_;
};

class DBusConnection
        : public DBusProxyConnection,
          public std::enable_shared_from_this<DBusConnection> {
public:
    COMMONAPI_EXPORT static std::shared_ptr<DBusConnection> getBus(
        const DBusType_t &_type, const ConnectionId_t& _connectionId);
    COMMONAPI_EXPORT static std::shared_ptr<DBusConnection> wrap(
        ::DBusConnection *_connection, const ConnectionId_t& _connectionId);

    COMMONAPI_EXPORT DBusConnection(DBusType_t _type,
        const ConnectionId_t& _connectionId);
    COMMONAPI_EXPORT DBusConnection(const DBusConnection&) = delete;
    COMMONAPI_EXPORT DBusConnection(::DBusConnection* libDbusConnection,
        const ConnectionId_t& _connectionId);
    COMMONAPI_EXPORT virtual ~DBusConnection();

    COMMONAPI_EXPORT DBusConnection& operator=(const DBusConnection&) = delete;

    COMMONAPI_EXPORT DBusType_t getBusType() const;

    COMMONAPI_EXPORT bool connect(bool startDispatchThread = true);
    COMMONAPI_EXPORT bool connect(DBusError& dbusError, bool startDispatchThread = true);
    COMMONAPI_EXPORT void disconnect();

    COMMONAPI_EXPORT virtual bool isConnected() const;

    COMMONAPI_EXPORT virtual ConnectionStatusEvent& getConnectionStatusEvent();

    COMMONAPI_EXPORT virtual bool requestServiceNameAndBlock(const std::string& serviceName) const;
    COMMONAPI_EXPORT virtual bool releaseServiceName(const std::string& serviceName) const;

    COMMONAPI_EXPORT bool sendDBusMessage(const DBusMessage& dbusMessage/*, uint32_t* allocatedSerial = NULL*/) const;

    COMMONAPI_EXPORT std::future<CallStatus> sendDBusMessageWithReplyAsync(
            const DBusMessage& dbusMessage,
            std::unique_ptr<DBusMessageReplyAsyncHandler> dbusMessageReplyAsyncHandler,
            const CommonAPI::CallInfo *_info) const;

    COMMONAPI_EXPORT DBusMessage sendDBusMessageWithReplyAndBlock(const DBusMessage& dbusMessage,
                                                 DBusError& dbusError,
                                                 const CommonAPI::CallInfo *_info) const;

    COMMONAPI_EXPORT virtual bool addObjectManagerSignalMemberHandler(const std::string& dbusBusName,
                                                     DBusSignalHandler* dbusSignalHandler);
    COMMONAPI_EXPORT virtual bool removeObjectManagerSignalMemberHandler(const std::string& dbusBusName,
                                                        DBusSignalHandler* dbusSignalHandler);

    COMMONAPI_EXPORT DBusSignalHandlerToken addSignalMemberHandler(const std::string& objectPath,
                                                  const std::string& interfaceName,
                                                  const std::string& interfaceMemberName,
                                                  const std::string& inuint32_tterfaceMemberSignature,
                                                  DBusSignalHandler* dbusSignalHandler,
                                                  const bool justAddFilter = false);

    COMMONAPI_EXPORT DBusProxyConnection::DBusSignalHandlerToken subscribeForSelectiveBroadcast(bool& subscriptionAccepted,
                                                                               const std::string& objectPath,
                                                                               const std::string& interfaceName,
                                                                               const std::string& interfaceMemberName,
                                                                               const std::string& interfaceMemberSignature,
                                                                               DBusSignalHandler* dbusSignalHandler,
                                                                               DBusProxy* callingProxy);

    COMMONAPI_EXPORT void unsubscribeFromSelectiveBroadcast(const std::string& eventName,
                                          DBusProxyConnection::DBusSignalHandlerToken subscription,
                                          DBusProxy* callingProxy,
                                          const DBusSignalHandler* dbusSignalHandler);

    COMMONAPI_EXPORT void registerObjectPath(const std::string& objectPath);
    COMMONAPI_EXPORT void unregisterObjectPath(const std::string& objectPath);

    COMMONAPI_EXPORT bool removeSignalMemberHandler(const DBusSignalHandlerToken& dbusSignalHandlerToken,
                                   const DBusSignalHandler* dbusSignalHandler = NULL);
    COMMONAPI_EXPORT bool readWriteDispatch(int timeoutMilliseconds = -1);

    COMMONAPI_EXPORT virtual const std::shared_ptr<DBusObjectManager> getDBusObjectManager();

    COMMONAPI_EXPORT void setObjectPathMessageHandler(DBusObjectPathMessageHandler);
    COMMONAPI_EXPORT bool isObjectPathMessageHandlerSet();

    COMMONAPI_EXPORT virtual bool attachMainLoopContext(std::weak_ptr<MainLoopContext>);

    COMMONAPI_EXPORT bool isDispatchReady();
    COMMONAPI_EXPORT bool singleDispatch();

    COMMONAPI_EXPORT virtual bool hasDispatchThread();

    COMMONAPI_EXPORT virtual const ConnectionId_t& getConnectionId() const;

#ifdef COMMONAPI_DBUS_TEST
    inline std::weak_ptr<DBusMainloop> getLoop() { return loop_; }
#endif

    typedef std::tuple<std::string, std::string, std::string> DBusSignalMatchRuleTuple;
    typedef std::pair<uint32_t, std::string> DBusSignalMatchRuleMapping;
    typedef std::unordered_map<DBusSignalMatchRuleTuple, DBusSignalMatchRuleMapping> DBusSignalMatchRulesMap;
 private:
    COMMONAPI_EXPORT void dispatch();
    COMMONAPI_EXPORT void suspendDispatching() const;
    COMMONAPI_EXPORT void resumeDispatching() const;

    std::thread* dispatchThread_;
    bool stopDispatching_;

    std::weak_ptr<MainLoopContext> mainLoopContext_;
    DispatchSource* dispatchSource_;
    WatchContext* watchContext_;

    mutable std::recursive_mutex sendLock_;
    mutable bool pauseDispatching_;
    mutable std::mutex dispatchSuspendLock_;

    COMMONAPI_EXPORT void addLibdbusSignalMatchRule(const std::string& objectPath,
            const std::string& interfaceName,
            const std::string& interfaceMemberName,
            const bool justAddFilter = false);

    COMMONAPI_EXPORT void removeLibdbusSignalMatchRule(const std::string& objectPath,
            const std::string& interfaceName,
            const std::string& interfaceMemberName);

    COMMONAPI_EXPORT void initLibdbusSignalFilterAfterConnect();
    ::DBusHandlerResult onLibdbusSignalFilter(::DBusMessage* libdbusMessage);

    COMMONAPI_EXPORT void initLibdbusObjectPathHandlerAfterConnect();
    ::DBusHandlerResult onLibdbusObjectPathMessage(::DBusMessage* libdbusMessage);

    COMMONAPI_EXPORT static DBusMessage convertToDBusMessage(::DBusPendingCall* _libdbusPendingCall,
            CallStatus& _callStatus);
    COMMONAPI_EXPORT static void onLibdbusPendingCallNotifyThunk(::DBusPendingCall* libdbusPendingCall, void* userData);
    COMMONAPI_EXPORT static void onLibdbusDataCleanup(void* userData);

    COMMONAPI_EXPORT static ::DBusHandlerResult onLibdbusObjectPathMessageThunk(::DBusConnection* libdbusConnection,
            ::DBusMessage* libdbusMessage,
            void* userData);

    COMMONAPI_EXPORT static ::DBusHandlerResult onLibdbusSignalFilterThunk(::DBusConnection* libdbusConnection,
            ::DBusMessage* libdbusMessage,
            void* userData);

    COMMONAPI_EXPORT static dbus_bool_t onAddWatch(::DBusWatch* libdbusWatch, void* data);
    COMMONAPI_EXPORT static void onRemoveWatch(::DBusWatch* libdbusWatch, void* data);
    COMMONAPI_EXPORT static void onToggleWatch(::DBusWatch* libdbusWatch, void* data);

    COMMONAPI_EXPORT static dbus_bool_t onAddTimeout(::DBusTimeout* dbus_timeout, void* data);
    COMMONAPI_EXPORT static void onRemoveTimeout(::DBusTimeout* dbus_timeout, void* data);
    COMMONAPI_EXPORT static void onToggleTimeout(::DBusTimeout* dbus_timeout, void* data);

    COMMONAPI_EXPORT static void onWakeupMainContext(void* data);

    COMMONAPI_EXPORT void enforceAsynchronousTimeouts() const;
    COMMONAPI_EXPORT static const DBusObjectPathVTable* getDBusObjectPathVTable();

    COMMONAPI_EXPORT bool sendPendingSelectiveSubscription(DBusProxy* proxy, std::string methodName);

    ::DBusConnection* connection_;
    mutable std::mutex connectionGuard_;

    std::mutex signalGuard_;
    std::mutex objectManagerGuard_;
    std::mutex serviceRegistryGuard_;

    DBusType_t busType_;

    std::shared_ptr<DBusObjectManager> dbusObjectManager_;

    DBusConnectionStatusEvent dbusConnectionStatusEvent_;

    DBusSignalMatchRulesMap dbusSignalMatchRulesMap_;

    DBusSignalHandlerTable dbusSignalHandlerTable_;

    std::unordered_map<std::string, size_t> dbusObjectManagerSignalMatchRulesMap_;
    std::unordered_multimap<std::string, DBusSignalHandler*> dbusObjectManagerSignalHandlerTable_;
    std::mutex dbusObjectManagerSignalGuard_;

    COMMONAPI_EXPORT bool addObjectManagerSignalMatchRule(const std::string& dbusBusName);
    COMMONAPI_EXPORT bool removeObjectManagerSignalMatchRule(const std::string& dbusBusName);

    COMMONAPI_EXPORT bool addLibdbusSignalMatchRule(const std::string& dbusMatchRule);
    COMMONAPI_EXPORT bool removeLibdbusSignalMatchRule(const std::string& dbusMatchRule);

    std::atomic_size_t libdbusSignalMatchRulesCount_;

    // objectPath, referenceCount
    typedef std::unordered_map<std::string, uint32_t> LibdbusRegisteredObjectPathHandlersTable;
    LibdbusRegisteredObjectPathHandlersTable libdbusRegisteredObjectPaths_;

    DBusObjectPathMessageHandler dbusObjectMessageHandler_;

    mutable std::unordered_map<std::string, uint16_t> connectionNameCount_;

    typedef std::pair<
            DBusPendingCall*,
            std::tuple<
                std::chrono::time_point<std::chrono::high_resolution_clock>,
                DBusMessageReplyAsyncHandler*,
                DBusMessage
            >
        > TimeoutMapElement;
    mutable std::map<
            DBusPendingCall*,
            std::tuple<
                std::chrono::time_point<std::chrono::high_resolution_clock>,
                DBusMessageReplyAsyncHandler*,
                DBusMessage
            >
        > timeoutMap_;

    typedef std::tuple<
                DBusMessageReplyAsyncHandler *,
                DBusMessage,
                CommonAPI::CallStatus,
                ::DBusPendingCall*
            > MainloopTimeout_t;
    mutable std::list<MainloopTimeout_t> mainloopTimeouts_;
    mutable std::mutex mainloopTimeoutsMutex_;

    mutable std::mutex enforceTimeoutMutex_;
    mutable std::condition_variable enforceTimeoutCondition_;

    mutable std::shared_ptr<std::thread> enforcerThread_;
    mutable std::mutex enforcerThreadMutex_;
    bool enforcerThreadCancelled_;
    ConnectionId_t connectionId_;

    std::shared_ptr<DBusMainLoop> loop_;

    // set contains asyncHandlers with infinite timeout
    mutable std::set<DBusMessageReplyAsyncHandler*> timeoutInfiniteAsyncHandlers_;
    mutable std::mutex timeoutInfiniteAsyncHandlersMutex_;

};


} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUS_CONNECTION_HPP_
