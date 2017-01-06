// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSINSTANCEAVAILABILITYSTATUSCHANGED_EVENT_HPP_
#define COMMONAPI_DBUS_DBUSINSTANCEAVAILABILITYSTATUSCHANGED_EVENT_HPP_

#include <functional>
#include <future>
#include <string>
#include <vector>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/ProxyManager.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>
#include <CommonAPI/DBus/DBusObjectManagerStub.hpp>
#include <CommonAPI/DBus/DBusTypes.hpp>

namespace CommonAPI {
namespace DBus {

// TODO Check to move logic to DBusServiceRegistry, now every proxy will deserialize the messages!
class DBusInstanceAvailabilityStatusChangedEvent:
                public ProxyManager::InstanceAvailabilityStatusChangedEvent {
 public:

    typedef std::function<void(const CallStatus &, const std::vector<DBusAddress> &)> GetAvailableServiceInstancesCallback;

    COMMONAPI_EXPORT DBusInstanceAvailabilityStatusChangedEvent(DBusProxy &_proxy,
                                               const std::string &_dbusInterfaceName,
                                               const std::string &_capiInterfaceName);

    COMMONAPI_EXPORT virtual ~DBusInstanceAvailabilityStatusChangedEvent();

    COMMONAPI_EXPORT void getAvailableServiceInstances(CommonAPI::CallStatus &_status, std::vector<DBusAddress> &_availableServiceInstances);
    COMMONAPI_EXPORT std::future<CallStatus> getAvailableServiceInstancesAsync(GetAvailableServiceInstancesCallback _callback);

    COMMONAPI_EXPORT void getServiceInstanceAvailabilityStatus(const std::string &_instance,
                                       CallStatus &_callStatus,
                                       AvailabilityStatus &_availabilityStatus);
    COMMONAPI_EXPORT std::future<CallStatus> getServiceInstanceAvailabilityStatusAsync(const std::string& _instance,
            ProxyManager::GetInstanceAvailabilityStatusCallback _callback);

 protected:

    class SignalHandler : public DBusProxyConnection::DBusSignalHandler {
    public:
        COMMONAPI_EXPORT SignalHandler(DBusInstanceAvailabilityStatusChangedEvent* _instanceAvblStatusEvent);
        COMMONAPI_EXPORT virtual void onSignalDBusMessage(const DBusMessage& dbusMessage);
    private:
        DBusInstanceAvailabilityStatusChangedEvent* instanceAvblStatusEvent_;
    };

    virtual void onFirstListenerAdded(const Listener&);
    virtual void onLastListenerRemoved(const Listener&);

 private:

    void onInterfacesAddedSignal(const DBusMessage &_message);

    void onInterfacesRemovedSignal(const DBusMessage &_message);

    void notifyInterfaceStatusChanged(const std::string &_objectPath,
                                      const std::string &_interfaceName,
                                      const AvailabilityStatus &_availability);

    bool addInterface(const std::string &_dbusObjectPath,
                      const std::string &_dbusInterfaceName);
    bool removeInterface(const std::string &_dbusObjectPath,
                         const std::string &_dbusInterfaceName);

    void serviceInstancesAsyncCallback(std::shared_ptr<Proxy> _proxy,
                                       const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict _dict,
                                       GetAvailableServiceInstancesCallback &_call,
                                       std::shared_ptr<std::promise<CallStatus> > &_promise);

    void translate(const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict &_dict,
                                     std::vector<DBusAddress> &_serviceInstances);

    std::shared_ptr<SignalHandler> signalHandler_;
    DBusProxy &proxy_;
    std::weak_ptr<DBusProxy> proxyWeakPtr_;
    std::string observedDbusInterfaceName_;
    std::string observedCapiInterfaceName_;
    DBusProxyConnection::DBusSignalHandlerToken interfacesAddedSubscription_;
    DBusProxyConnection::DBusSignalHandlerToken interfacesRemovedSubscription_;
    std::mutex interfacesMutex_;
    std::map<std::string, std::set<std::string>> interfaces_;
    const std::shared_ptr<DBusServiceRegistry> registry_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSINSTANCEAVAILABILITYSTATUSCHANGEDEVENT_HPP_
