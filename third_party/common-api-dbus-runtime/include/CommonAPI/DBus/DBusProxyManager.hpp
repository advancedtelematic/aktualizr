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

class DBusProxyManager: public ProxyManager {
public:
    COMMONAPI_EXPORT DBusProxyManager(DBusProxy &_proxy,
                     const std::string &_interfaceName);

    COMMONAPI_EXPORT const std::string &getDomain() const;
    COMMONAPI_EXPORT const std::string &getInterface() const;
    COMMONAPI_EXPORT const ConnectionId_t &getConnectionId() const;

    COMMONAPI_EXPORT virtual void getAvailableInstances(CommonAPI::CallStatus &, std::vector<std::string> &_instances);
    COMMONAPI_EXPORT virtual std::future<CallStatus> getAvailableInstancesAsync(GetAvailableInstancesCallback _callback);

    COMMONAPI_EXPORT virtual void getInstanceAvailabilityStatus(const std::string &_address,
                                               CallStatus &_callStatus,
                                               AvailabilityStatus &_availabilityStatus);

    COMMONAPI_EXPORT  virtual std::future<CallStatus> getInstanceAvailabilityStatusAsync(
                                        const std::string&,
                                        GetInstanceAvailabilityStatusCallback callback);

    COMMONAPI_EXPORT virtual InstanceAvailabilityStatusChangedEvent& getInstanceAvailabilityStatusChangedEvent();

private:
    COMMONAPI_EXPORT void instancesAsyncCallback(const CommonAPI::CallStatus& status,
                                const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict& dict,
                                GetAvailableInstancesCallback& call);

    COMMONAPI_EXPORT void instanceAliveAsyncCallback(const AvailabilityStatus &_alive,
                                    GetInstanceAvailabilityStatusCallback &_call,
                                    std::shared_ptr<std::promise<CallStatus>> &_status);

    COMMONAPI_EXPORT void translateCommonApiAddresses(const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict &_dict,
                                     std::vector<std::string> &_instances);

    DBusProxy &proxy_;
    DBusInstanceAvailabilityStatusChangedEvent instanceAvailabilityStatusEvent_;
    const std::string interfaceId_;
    const std::shared_ptr<DBusServiceRegistry> registry_;
    ConnectionId_t connectionId_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_PROXYMANAGER_HPP_
