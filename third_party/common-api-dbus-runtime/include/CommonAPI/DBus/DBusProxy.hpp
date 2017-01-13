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
    virtual void onListenerAdded(const Listener& listener, const Subscription subscription);

    DBusProxy* dbusProxy_;
};


class DBusProxy
        : public DBusProxyBase {
public:
    COMMONAPI_EXPORT DBusProxy(const DBusAddress &_address,
              const std::shared_ptr<DBusProxyConnection> &_connection);
    COMMONAPI_EXPORT virtual ~DBusProxy();

    COMMONAPI_EXPORT virtual ProxyStatusEvent& getProxyStatusEvent();
    COMMONAPI_EXPORT virtual InterfaceVersionAttribute& getInterfaceVersionAttribute();

    COMMONAPI_EXPORT virtual bool isAvailable() const;
    COMMONAPI_EXPORT virtual bool isAvailableBlocking() const;

    COMMONAPI_EXPORT DBusProxyConnection::DBusSignalHandlerToken subscribeForSelectiveBroadcastOnConnection(
              bool& subscriptionAccepted,
              const std::string& objectPath,
              const std::string& interfaceName,
              const std::string& interfaceMemberName,
              const std::string& interfaceMemberSignature,
              DBusProxyConnection::DBusSignalHandler* dbusSignalHandler);
    COMMONAPI_EXPORT void unsubscribeFromSelectiveBroadcast(const std::string& eventName,
                                           DBusProxyConnection::DBusSignalHandlerToken subscription,
                                           const DBusProxyConnection::DBusSignalHandler* dbusSignalHandler);

    COMMONAPI_EXPORT void init();

    COMMONAPI_EXPORT virtual DBusProxyConnection::DBusSignalHandlerToken addSignalMemberHandler(
            const std::string &objectPath,
            const std::string &interfaceName,
            const std::string &signalName,
            const std::string &signalSignature,
            DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
            const bool justAddFilter);

    COMMONAPI_EXPORT virtual DBusProxyConnection::DBusSignalHandlerToken addSignalMemberHandler(
            const std::string &objectPath,
            const std::string &interfaceName,
            const std::string &signalName,
            const std::string &signalSignature,
            const std::string &getMethodName,
            DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
            const bool justAddFilter);

    COMMONAPI_EXPORT virtual bool removeSignalMemberHandler(
            const DBusProxyConnection::DBusSignalHandlerToken &_dbusSignalHandlerToken,
            const DBusProxyConnection::DBusSignalHandler *_dbusSignalHandler = NULL);

    COMMONAPI_EXPORT virtual void getCurrentValueForSignalListener(
            const std::string &getMethodName,
            DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
            const uint32_t subscription);

    COMMONAPI_EXPORT virtual void freeDesktopGetCurrentValueForSignalListener(
            DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
            const uint32_t subscription,
            const std::string &interfaceName,
            const std::string &propertyName);

private:
    typedef std::tuple<
        const std::string,
        const std::string,
        const std::string,
        const std::string,
        const std::string,
        DBusProxyConnection::DBusSignalHandler*,
        const bool,
        bool
        > SignalMemberHandlerTuple;

    COMMONAPI_EXPORT DBusProxy(const DBusProxy &) = delete;

    COMMONAPI_EXPORT void onDBusServiceInstanceStatus(const AvailabilityStatus& availabilityStatus);
    COMMONAPI_EXPORT void signalMemberCallback(const CallStatus dbusMessageCallStatus,
            const DBusMessage& dbusMessage,
            DBusProxyConnection::DBusSignalHandler* dbusSignalHandlers,
            const uint32_t tag);
    COMMONAPI_EXPORT void signalInitialValueCallback(const CallStatus dbusMessageCallStatus,
            const DBusMessage& dbusMessage,
            DBusProxyConnection::DBusSignalHandler* dbusSignalHandlers,
            const uint32_t tag);
    COMMONAPI_EXPORT void addSignalMemberHandlerToQueue(SignalMemberHandlerTuple& _signalMemberHandler);

    DBusProxyStatusEvent dbusProxyStatusEvent_;
    DBusServiceRegistry::DBusServiceSubscription dbusServiceRegistrySubscription_;
    AvailabilityStatus availabilityStatus_;
    mutable std::mutex availabilityStatusMutex_;

    DBusReadonlyAttribute<InterfaceVersionAttribute> interfaceVersionAttribute_;

    std::shared_ptr<DBusServiceRegistry> dbusServiceRegistry_;

    mutable std::mutex availabilityMutex_;
    mutable std::condition_variable availabilityCondition_;

    std::list<SignalMemberHandlerTuple> signalMemberHandlerQueue_;
    CallInfo signalMemberHandlerInfo_;
    mutable std::mutex signalMemberHandlerQueueMutex_;

    std::map<std::string, DBusProxyConnection::DBusSignalHandler*> selectiveBroadcastHandlers;
    mutable std::mutex selectiveBroadcastHandlersMutex_;
};


} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSPROXY_HPP_

