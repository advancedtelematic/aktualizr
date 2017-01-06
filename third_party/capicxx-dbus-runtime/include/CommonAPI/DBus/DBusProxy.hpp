// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSPROXY_HPP_
#define COMMONAPI_DBUS_DBUSPROXY_HPP_

#include <functional>
#include <memory>
#include <string>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/DBus/DBusAttribute.hpp>
#include <CommonAPI/DBus/DBusServiceRegistry.hpp>

namespace CommonAPI {
namespace DBus {

class DBusProxyStatusEvent
        : public ProxyStatusEvent {
    friend class DBusProxy;

 public:
    DBusProxyStatusEvent(DBusProxy* dbusProxy);
    virtual ~DBusProxyStatusEvent() {}

 protected:
    virtual void onListenerAdded(const Listener& _listener, const Subscription _subscription);
    virtual void onListenerRemoved(const Listener &_listener, const Subscription _subscription);

    DBusProxy* dbusProxy_;

    std::recursive_mutex listenersMutex_;
    std::vector<std::pair<ProxyStatusEvent::Subscription, ProxyStatusEvent::Listener>> listeners_;
};


class COMMONAPI_EXPORT_CLASS_EXPLICIT DBusProxy
        : public DBusProxyBase,
          public std::enable_shared_from_this<DBusProxy> {
public:
    COMMONAPI_EXPORT DBusProxy(const DBusAddress &_address,
              const std::shared_ptr<DBusProxyConnection> &_connection);
    COMMONAPI_EXPORT virtual ~DBusProxy();

    COMMONAPI_EXPORT AvailabilityStatus getAvailabilityStatus() const;

    COMMONAPI_EXPORT virtual ProxyStatusEvent& getProxyStatusEvent();
    COMMONAPI_EXPORT virtual InterfaceVersionAttribute& getInterfaceVersionAttribute();

    COMMONAPI_EXPORT virtual bool isAvailable() const;
    COMMONAPI_EXPORT virtual bool isAvailableBlocking() const;
    COMMONAPI_EXPORT virtual std::future<AvailabilityStatus> isAvailableAsync(
                isAvailableAsyncCallback _callback,
                const CallInfo *_info) const;

    COMMONAPI_EXPORT void subscribeForSelectiveBroadcastOnConnection(
              const std::string& objectPath,
              const std::string& interfaceName,
              const std::string& interfaceMemberName,
              const std::string& interfaceMemberSignature,
              std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
			  uint32_t tag);

    COMMONAPI_EXPORT void insertSelectiveSubscription(
            const std::string& interfaceMemberName,
            std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
            uint32_t tag, std::string interfaceMemberSignature);
    COMMONAPI_EXPORT void unsubscribeFromSelectiveBroadcast(const std::string& eventName,
                                           DBusProxyConnection::DBusSignalHandlerToken subscription,
                                           const DBusProxyConnection::DBusSignalHandler* dbusSignalHandler);

    COMMONAPI_EXPORT void init();

    COMMONAPI_EXPORT virtual DBusProxyConnection::DBusSignalHandlerToken addSignalMemberHandler(
            const std::string &objectPath,
            const std::string &interfaceName,
            const std::string &signalName,
            const std::string &signalSignature,
            std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
            const bool justAddFilter);

    COMMONAPI_EXPORT virtual DBusProxyConnection::DBusSignalHandlerToken addSignalMemberHandler(
            const std::string &objectPath,
            const std::string &interfaceName,
            const std::string &signalName,
            const std::string &signalSignature,
            const std::string &getMethodName,
            std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
            const bool justAddFilter);

    COMMONAPI_EXPORT virtual bool removeSignalMemberHandler(
            const DBusProxyConnection::DBusSignalHandlerToken &_dbusSignalHandlerToken,
            const DBusProxyConnection::DBusSignalHandler* _dbusSignalHandler = NULL);

    COMMONAPI_EXPORT virtual void getCurrentValueForSignalListener(
            const std::string &getMethodName,
            std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
            const uint32_t subscription);

    COMMONAPI_EXPORT virtual void freeDesktopGetCurrentValueForSignalListener(
            std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
            const uint32_t subscription,
            const std::string &interfaceName,
            const std::string &propertyName);

    COMMONAPI_EXPORT static void notifySpecificListener(std::weak_ptr<DBusProxy> _dbusProxy,
                                                         const ProxyStatusEvent::Listener &_listener,
                                                         const ProxyStatusEvent::Subscription _subscription);

private:
    typedef std::tuple<
        const std::string,
        const std::string,
        const std::string,
        const std::string,
        const std::string,
        std::weak_ptr<DBusProxyConnection::DBusSignalHandler>,
        const bool,
        bool
        > SignalMemberHandlerTuple;

    COMMONAPI_EXPORT DBusProxy(const DBusProxy &) = delete;

    COMMONAPI_EXPORT void onDBusServiceInstanceStatus(std::shared_ptr<DBusProxy> _proxy,
                                                      const AvailabilityStatus& availabilityStatus);
    COMMONAPI_EXPORT void signalMemberCallback(const CallStatus dbusMessageCallStatus,
            const DBusMessage& dbusMessage,
            std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandlers,
            const uint32_t tag);
    COMMONAPI_EXPORT void signalInitialValueCallback(const CallStatus dbusMessageCallStatus,
            const DBusMessage& dbusMessage,
            std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandlers,
            const uint32_t tag);
    COMMONAPI_EXPORT void addSignalMemberHandlerToQueue(SignalMemberHandlerTuple& _signalMemberHandler);

    COMMONAPI_EXPORT void availabilityTimeoutThreadHandler() const;

    DBusProxyStatusEvent dbusProxyStatusEvent_;
    DBusServiceRegistry::DBusServiceSubscription dbusServiceRegistrySubscription_;
    AvailabilityStatus availabilityStatus_;

    DBusReadonlyAttribute<InterfaceVersionAttribute> interfaceVersionAttribute_;

    std::shared_ptr<DBusServiceRegistry> dbusServiceRegistry_;

    mutable std::mutex availabilityMutex_;
    mutable std::condition_variable availabilityCondition_;

    std::list<SignalMemberHandlerTuple> signalMemberHandlerQueue_;
    mutable std::mutex signalMemberHandlerQueueMutex_;

    std::map<std::string,
            std::tuple<std::weak_ptr<DBusProxyConnection::DBusSignalHandler>, uint32_t,
                    std::string>> selectiveBroadcastHandlers;
    mutable std::mutex selectiveBroadcastHandlersMutex_;

    mutable std::shared_ptr<std::thread> availabilityTimeoutThread_;
    mutable std::mutex availabilityTimeoutThreadMutex_;
    mutable std::mutex timeoutsMutex_;
    mutable std::condition_variable availabilityTimeoutCondition_;

    typedef std::tuple<
                std::chrono::steady_clock::time_point,
                isAvailableAsyncCallback,
                std::promise<AvailabilityStatus>
                > AvailabilityTimeout_t;
    mutable std::list<AvailabilityTimeout_t> timeouts_;
};


} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSPROXY_HPP_

