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
        const std::string &_interfaceId)
    : proxy_(_proxy),
      instanceAvailabilityStatusEvent_(_proxy, _interfaceId),
      interfaceId_(_interfaceId),
      registry_(DBusServiceRegistry::get(_proxy.getDBusConnection()))
{
}

const std::string &
DBusProxyManager::getDomain() const {
    static std::string domain("local");
    return domain;
}

const std::string &
DBusProxyManager::getInterface() const {
    return interfaceId_;
}

const ConnectionId_t &
DBusProxyManager::getConnectionId() const {
    // Every DBusProxyConnection is created as DBusConnection
    // in Factory::getConnection and is only stored as DBusProxyConnection
    return std::static_pointer_cast<DBusConnection>(proxy_.getDBusConnection())->getConnectionId();
}

void
DBusProxyManager::instancesAsyncCallback(
        const CommonAPI::CallStatus &_status,
        const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict &_dict,
        GetAvailableInstancesCallback &_call) {
    std::vector<std::string> result;
    if (_status == CommonAPI::CallStatus::SUCCESS) {
        translateCommonApiAddresses(_dict, result);
    }
    _call(_status, result);
}

void
DBusProxyManager::getAvailableInstances(
        CommonAPI::CallStatus &_status,
        std::vector<std::string> &_availableInstances) {
    DBusObjectManagerStub::DBusObjectPathAndInterfacesDict dict;

    DBusProxyHelper<
        DBusSerializableArguments<>,
        DBusSerializableArguments<
            DBusObjectManagerStub::DBusObjectPathAndInterfacesDict
        >
    >::callMethodWithReply(proxy_,
                           DBusObjectManagerStub::getInterfaceName(),
                           "GetManagedObjects",
                           "",
                           &defaultCallInfo,
                           _status,
                           dict);

    if (_status == CallStatus::SUCCESS) {
        translateCommonApiAddresses(dict, _availableInstances);
    }
}

std::future<CallStatus>
DBusProxyManager::getAvailableInstancesAsync(
        GetAvailableInstancesCallback _callback) {
    return CommonAPI::DBus::DBusProxyHelper<
                CommonAPI::DBus::DBusSerializableArguments<>,
                CommonAPI::DBus::DBusSerializableArguments<
                    DBusObjectManagerStub::DBusObjectPathAndInterfacesDict
                >
           >::callMethodAsync(
                   proxy_,
                   DBusObjectManagerStub::getInterfaceName(),
                   "GetManagedObjects",
                   "",
                   &defaultCallInfo,
                   std::move(
                           std::bind(
                                   &DBusProxyManager::instancesAsyncCallback,
                                   this,
                                   std::placeholders::_1, std::placeholders::_2,
                                   _callback
                           )
                   ),
                         std::tuple<DBusObjectManagerStub::DBusObjectPathAndInterfacesDict>());
}

void
DBusProxyManager::getInstanceAvailabilityStatus(
        const std::string &_address,
        CallStatus &_callStatus,
        AvailabilityStatus &_availabilityStatus) {

    CommonAPI::Address itsAddress("local", interfaceId_, _address);
    DBusAddress itsDBusAddress;
    DBusAddressTranslator::get()->translate(itsAddress, itsDBusAddress);

    _availabilityStatus = AvailabilityStatus::NOT_AVAILABLE;
    if (registry_->isServiceInstanceAlive(
            itsDBusAddress.getInterface(),
            itsDBusAddress.getService(),
            itsDBusAddress.getObjectPath())) {
        _availabilityStatus = AvailabilityStatus::AVAILABLE;
    }
    _callStatus = CallStatus::SUCCESS;
}

void
DBusProxyManager::instanceAliveAsyncCallback(
        const AvailabilityStatus &_alive,
        GetInstanceAvailabilityStatusCallback &_call,
        std::shared_ptr<std::promise<CallStatus> > &_status) {
    _call(CallStatus::SUCCESS, _alive);
    _status->set_value(CallStatus::SUCCESS);
}

std::future<CallStatus>
DBusProxyManager::getInstanceAvailabilityStatusAsync(
        const std::string &_instance,
        GetInstanceAvailabilityStatusCallback _callback) {

    CommonAPI::Address itsAddress("local", interfaceId_, _instance);

    std::shared_ptr<std::promise<CallStatus> > promise = std::make_shared<std::promise<CallStatus>>();
    registry_->subscribeAvailabilityListener(
                    itsAddress.getAddress(),
                    std::bind(&DBusProxyManager::instanceAliveAsyncCallback,
                              this,
                              std::placeholders::_1,
                              _callback,
                              promise)
    );

    return promise->get_future();
}

DBusProxyManager::InstanceAvailabilityStatusChangedEvent &
DBusProxyManager::getInstanceAvailabilityStatusChangedEvent() {
    return instanceAvailabilityStatusEvent_;
}

void
DBusProxyManager::translateCommonApiAddresses(
        const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict &_dict,
        std::vector<std::string> &_instances) {

    CommonAPI::Address itsAddress;
    DBusAddress itsDBusAddress;

    for (const auto &objectPathIter : _dict) {
        itsDBusAddress.setObjectPath(objectPathIter.first);

        const auto &interfacesDict = objectPathIter.second;
        for (const auto &interfaceIter : interfacesDict) {

            // return only those addresses whose interface matches with ours
            if (interfaceIter.first == interfaceId_) {
                itsDBusAddress.setInterface(interfaceIter.first);
                DBusAddressTranslator::get()->translate(itsDBusAddress, itsAddress);
                _instances.push_back(itsAddress.getInstance());
            }
        }
    }
}

} // namespace DBus
}// namespace CommonAPI
