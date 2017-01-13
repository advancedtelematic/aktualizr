// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_PROXYMANAGER_HPP_
#define COMMONAPI_DBUS_PROXYMANAGER_HPP_

#include <functional>
#include <future>
#include <string>
#include <vector>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/ProxyManager.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>
#include <CommonAPI/DBus/DBusFactory.hpp>
#include <CommonAPI/DBus/DBusObjectManagerStub.hpp>
#include <CommonAPI/DBus/DBusInstanceAvailabilityStatusChangedEvent.hpp>
#include <CommonAPI/DBus/DBusConnection.hpp>

namespace CommonAPI {
namespace DBus {

class COMMONAPI_EXPORT_CLASS_EXPLICIT DBusProxyManager: public ProxyManager {
public:
    COMMONAPI_EXPORT DBusProxyManager(DBusProxy &_proxy,
                     const std::string &_dbusInterfaceName,
                     const std::string &_capiInterfaceName);

    COMMONAPI_EXPORT const std::string &getDomain() const;
    COMMONAPI_EXPORT const std::string &getInterface() const;
    COMMONAPI_EXPORT const ConnectionId_t &getConnectionId() const;

    COMMONAPI_EXPORT virtual void getAvailableInstances(CommonAPI::CallStatus &_status, std::vector<std::string> &_instances);
    COMMONAPI_EXPORT virtual std::future<CallStatus> getAvailableInstancesAsync(GetAvailableInstancesCallback _callback);

    COMMONAPI_EXPORT virtual void getInstanceAvailabilityStatus(const std::string &_instance,
                                               CallStatus &_callStatus,
                                               AvailabilityStatus &_availabilityStatus);

    COMMONAPI_EXPORT  virtual std::future<CallStatus> getInstanceAvailabilityStatusAsync(
                                        const std::string& _instance,
                                        GetInstanceAvailabilityStatusCallback _callback);

    COMMONAPI_EXPORT virtual InstanceAvailabilityStatusChangedEvent& getInstanceAvailabilityStatusChangedEvent();

private:
    COMMONAPI_EXPORT void instancesAsyncCallback(std::shared_ptr<Proxy> _proxy,
                                                 const CommonAPI::CallStatus &_status,
                                                 const std::vector<DBusAddress> &_availableServiceInstances,
                                                 GetAvailableInstancesCallback &_call);

    COMMONAPI_EXPORT void translate(const std::vector<DBusAddress> &_serviceInstances,
                                     std::vector<std::string> &_instances);

    DBusProxy &proxy_;
    DBusInstanceAvailabilityStatusChangedEvent instanceAvailabilityStatusEvent_;
    const std::string dbusInterfaceId_;
    const std::string capiInterfaceId_;
    ConnectionId_t connectionId_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_PROXYMANAGER_HPP_
