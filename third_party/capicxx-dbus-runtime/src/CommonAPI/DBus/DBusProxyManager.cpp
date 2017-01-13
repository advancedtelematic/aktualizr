// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/Runtime.hpp>
#include <CommonAPI/DBus/DBusAddressTranslator.hpp>
#include <CommonAPI/DBus/DBusProxyManager.hpp>

namespace CommonAPI {
namespace DBus {

DBusProxyManager::DBusProxyManager(
        DBusProxy &_proxy,
        const std::string &_dbusInterfaceId,
        const std::string &_capiInterfaceId)
    : proxy_(_proxy),
      instanceAvailabilityStatusEvent_(_proxy, _dbusInterfaceId, _capiInterfaceId),
      dbusInterfaceId_(_dbusInterfaceId),
      capiInterfaceId_(_capiInterfaceId)
{
}

const std::string &
DBusProxyManager::getDomain() const {
    static std::string domain("local");
    return domain;
}

const std::string &
DBusProxyManager::getInterface() const {
    return capiInterfaceId_;
}

const ConnectionId_t &
DBusProxyManager::getConnectionId() const {
    // Every DBusProxyConnection is created as DBusConnection
    // in Factory::getConnection and is only stored as DBusProxyConnection
    return std::static_pointer_cast<DBusConnection>(proxy_.getDBusConnection())->getConnectionId();
}

void
DBusProxyManager::instancesAsyncCallback(
        std::shared_ptr<Proxy> _proxy,
        const CommonAPI::CallStatus &_status,
        const std::vector<DBusAddress> &_availableServiceInstances,
        GetAvailableInstancesCallback &_call) {
    (void)_proxy;
    std::vector<std::string> itsAvailableInstances;
    if (_status == CommonAPI::CallStatus::SUCCESS) {
        translate(_availableServiceInstances, itsAvailableInstances);
    }
    _call(_status, itsAvailableInstances);
}

void
DBusProxyManager::getAvailableInstances(
        CommonAPI::CallStatus &_status,
        std::vector<std::string> &_availableInstances) {
    _availableInstances.clear();
    std::vector<DBusAddress> itsAvailableServiceInstances;
    instanceAvailabilityStatusEvent_.getAvailableServiceInstances(_status, itsAvailableServiceInstances);
    translate(itsAvailableServiceInstances, _availableInstances);
}

std::future<CallStatus>
DBusProxyManager::getAvailableInstancesAsync(
        GetAvailableInstancesCallback _callback) {
    return instanceAvailabilityStatusEvent_.getAvailableServiceInstancesAsync(std::bind(
            &DBusProxyManager::instancesAsyncCallback,
            this,
            proxy_.shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2,
            _callback));
}

void
DBusProxyManager::getInstanceAvailabilityStatus(
        const std::string &_instance,
        CallStatus &_callStatus,
        AvailabilityStatus &_availabilityStatus) {
    instanceAvailabilityStatusEvent_.getServiceInstanceAvailabilityStatus(_instance,
            _callStatus,
            _availabilityStatus);
}

std::future<CallStatus>
DBusProxyManager::getInstanceAvailabilityStatusAsync(
        const std::string &_instance,
        GetInstanceAvailabilityStatusCallback _callback) {
    return instanceAvailabilityStatusEvent_.getServiceInstanceAvailabilityStatusAsync(_instance, _callback);
}

DBusProxyManager::InstanceAvailabilityStatusChangedEvent &
DBusProxyManager::getInstanceAvailabilityStatusChangedEvent() {
    return instanceAvailabilityStatusEvent_;
}

void DBusProxyManager::translate(const std::vector<DBusAddress> &_serviceInstances,
                                 std::vector<std::string> &_instances) {
    CommonAPI::Address itsCapiAddress;
    for(auto itsDbusAddress : _serviceInstances) {
        DBusAddressTranslator::get()->translate(itsDbusAddress, itsCapiAddress);
        _instances.push_back(itsCapiAddress.getInstance());
    }
}

} // namespace DBus
}// namespace CommonAPI
