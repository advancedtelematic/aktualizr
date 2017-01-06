// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/DBus/DBusInstanceAvailabilityStatusChangedEvent.hpp>

#include <CommonAPI/DBus/DBusAddressTranslator.hpp>

namespace CommonAPI {
namespace DBus {

DBusInstanceAvailabilityStatusChangedEvent::SignalHandler::SignalHandler(DBusInstanceAvailabilityStatusChangedEvent* _instanceAvblStatusEvent) :
        instanceAvblStatusEvent_(_instanceAvblStatusEvent) {

}

void DBusInstanceAvailabilityStatusChangedEvent::SignalHandler::onSignalDBusMessage(const DBusMessage& dbusMessage) {
    if (dbusMessage.hasMemberName("InterfacesAdded")) {
        instanceAvblStatusEvent_->onInterfacesAddedSignal(dbusMessage);
    } else if (dbusMessage.hasMemberName("InterfacesRemoved")) {
        instanceAvblStatusEvent_->onInterfacesRemovedSignal(dbusMessage);
   }
}

DBusInstanceAvailabilityStatusChangedEvent::DBusInstanceAvailabilityStatusChangedEvent(
        DBusProxy &_proxy,
        const std::string &_dbusInterfaceName,
        const std::string &_capiInterfaceName) :
                signalHandler_(std::make_shared<SignalHandler>(this)),
                proxy_(_proxy),
                observedCapiInterfaceName_(_capiInterfaceName),
                registry_(DBusServiceRegistry::get(_proxy.getDBusConnection())) {
    (void)_dbusInterfaceName;
    DBusAddress itsDBusAddress;
    std::string itsCapiAddress = "local:" + observedCapiInterfaceName_ + ":fakeInstance";
    DBusAddressTranslator::get()->translate(itsCapiAddress, itsDBusAddress);
    observedDbusInterfaceName_ = itsDBusAddress.getInterface();
}

DBusInstanceAvailabilityStatusChangedEvent::~DBusInstanceAvailabilityStatusChangedEvent() {
    proxy_.removeSignalMemberHandler(interfacesAddedSubscription_, signalHandler_.get());
    proxy_.removeSignalMemberHandler(interfacesRemovedSubscription_, signalHandler_.get());
}

void DBusInstanceAvailabilityStatusChangedEvent::getAvailableServiceInstances(
        CommonAPI::CallStatus &_status,
        std::vector<DBusAddress> &_availableServiceInstances) {

    _availableServiceInstances.clear();
    DBusObjectManagerStub::DBusObjectPathAndInterfacesDict itsAvailableServiceInstances;
    registry_->getAvailableServiceInstances(proxy_.getDBusAddress().getService(),
            proxy_.getDBusAddress().getObjectPath(),
            itsAvailableServiceInstances);

    _status = CommonAPI::CallStatus::SUCCESS;
    translate(itsAvailableServiceInstances, _availableServiceInstances);
}

std::future<CallStatus> DBusInstanceAvailabilityStatusChangedEvent::getAvailableServiceInstancesAsync(
        GetAvailableServiceInstancesCallback _callback) {

    std::shared_ptr<std::promise<CallStatus> > promise = std::make_shared<std::promise<CallStatus>>();
    registry_->getAvailableServiceInstancesAsync(std::bind(
            &DBusInstanceAvailabilityStatusChangedEvent::serviceInstancesAsyncCallback,
            this,
            proxy_.shared_from_this(),
            std::placeholders::_1,
            _callback,
            promise),
            proxy_.getDBusAddress().getService(),
            proxy_.getDBusAddress().getObjectPath());
    return promise->get_future();
}

void DBusInstanceAvailabilityStatusChangedEvent::getServiceInstanceAvailabilityStatus(
        const std::string &_instance,
        CallStatus &_callStatus,
        AvailabilityStatus &_availabilityStatus) {

    CommonAPI::Address itsAddress("local", observedCapiInterfaceName_, _instance);
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

std::future<CallStatus> DBusInstanceAvailabilityStatusChangedEvent::getServiceInstanceAvailabilityStatusAsync(
        const std::string& _instance,
        ProxyManager::GetInstanceAvailabilityStatusCallback _callback) {

    std::shared_ptr<std::promise<CallStatus> > promise = std::make_shared<std::promise<CallStatus>>();
    auto proxy = proxy_.shared_from_this();
    std::async(std::launch::async, [this, _instance, _callback, promise, proxy]() {
        CallStatus callStatus;
        AvailabilityStatus availabilityStatus;
        getServiceInstanceAvailabilityStatus(_instance, callStatus, availabilityStatus);
        _callback(callStatus, availabilityStatus);
        promise->set_value(callStatus);
    });

    return promise->get_future();
}

void DBusInstanceAvailabilityStatusChangedEvent::onFirstListenerAdded(const Listener&) {

    proxyWeakPtr_ = proxy_.shared_from_this();

    interfacesAddedSubscription_ = proxy_.addSignalMemberHandler(
                     proxy_.getDBusAddress().getObjectPath(),
                     DBusObjectManagerStub::getInterfaceName(),
                     "InterfacesAdded",
                     "oa{sa{sv}}",
                     signalHandler_,
                     false);

     interfacesRemovedSubscription_ = proxy_.addSignalMemberHandler(
                     proxy_.getDBusAddress().getObjectPath(),
                     DBusObjectManagerStub::getInterfaceName(),
                     "InterfacesRemoved",
                     "oas",
                     signalHandler_,
                     false);

     getAvailableServiceInstancesAsync([&](const CallStatus &_status,
             const std::vector<DBusAddress> &_availableServices) {
         if(_status == CallStatus::SUCCESS) {
             for(auto service : _availableServices) {
                 if(service.getInterface() != observedDbusInterfaceName_)
                     continue;
                 if(addInterface(service.getObjectPath(), observedDbusInterfaceName_))
                     notifyInterfaceStatusChanged(service.getObjectPath(), observedDbusInterfaceName_, AvailabilityStatus::AVAILABLE);
             }
         }
     });
}

void DBusInstanceAvailabilityStatusChangedEvent::onLastListenerRemoved(const Listener&) {
   proxy_.removeSignalMemberHandler(interfacesAddedSubscription_, signalHandler_.get());
   proxy_.removeSignalMemberHandler(interfacesRemovedSubscription_, signalHandler_.get());
}

void DBusInstanceAvailabilityStatusChangedEvent::onInterfacesAddedSignal(const DBusMessage &_message) {
    DBusInputStream dbusInputStream(_message);
    std::string dbusObjectPath;
    std::string dbusInterfaceName;
    DBusInterfacesAndPropertiesDict dbusInterfacesAndPropertiesDict;

    dbusInputStream >> dbusObjectPath;
    if (dbusInputStream.hasError()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__) + " failed to read object path");
    }

    dbusInputStream.beginReadMapOfSerializableStructs();
    while (!dbusInputStream.readMapCompleted()) {
        dbusInputStream.align(8);
        dbusInputStream >> dbusInterfaceName;
        dbusInputStream.skipMap();
        if (dbusInputStream.hasError()) {
            COMMONAPI_ERROR(std::string(__FUNCTION__) + " failed to read interface name");
        }
        if(auto itsProxy = proxyWeakPtr_.lock() &&
                dbusInterfaceName == observedDbusInterfaceName_ &&
                addInterface(dbusObjectPath, dbusInterfaceName)) {
            (void)itsProxy;
            notifyInterfaceStatusChanged(dbusObjectPath, dbusInterfaceName, AvailabilityStatus::AVAILABLE);
        }
    }
    dbusInputStream.endReadMapOfSerializableStructs();
}

void DBusInstanceAvailabilityStatusChangedEvent::onInterfacesRemovedSignal(const DBusMessage &_message) {
    DBusInputStream dbusInputStream(_message);
    std::string dbusObjectPath;
    std::vector<std::string> dbusInterfaceNames;

    dbusInputStream >> dbusObjectPath;
    if (dbusInputStream.hasError()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__) + " failed to read object path");
    }

    dbusInputStream >> dbusInterfaceNames;
    if (dbusInputStream.hasError()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__) + " failed to read interface names");
    }

    for (const auto& dbusInterfaceName : dbusInterfaceNames) {
        if(auto itsProxy = proxyWeakPtr_.lock() &&
                dbusInterfaceName == observedDbusInterfaceName_ &&
                removeInterface(dbusObjectPath, dbusInterfaceName)) {
            (void)itsProxy;
            notifyInterfaceStatusChanged(dbusObjectPath, dbusInterfaceName, AvailabilityStatus::NOT_AVAILABLE);
        }
    }
}

void DBusInstanceAvailabilityStatusChangedEvent::notifyInterfaceStatusChanged(
        const std::string &_objectPath,
        const std::string &_interfaceName,
        const AvailabilityStatus &_availability) {
    CommonAPI::Address itsAddress;
    DBusAddress itsDBusAddress(proxy_.getDBusAddress().getService(),
                               _objectPath,
                               _interfaceName);

    DBusAddressTranslator::get()->translate(itsDBusAddress, itsAddress);

    notifyListeners(itsAddress.getAddress(), _availability);
}

bool DBusInstanceAvailabilityStatusChangedEvent::addInterface(
        const std::string &_dbusObjectPath,
        const std::string &_dbusInterfaceName) {
    std::lock_guard<std::mutex> lock(interfacesMutex_);
    auto it = interfaces_.find(_dbusObjectPath);
    if (it == interfaces_.end()) {
        std::set<std::string> itsInterfaces;
        itsInterfaces.insert(_dbusInterfaceName);
        interfaces_[_dbusObjectPath] = itsInterfaces;
        return true;
    } else {
        if(it->second.insert(_dbusInterfaceName).second)
            return true;
    }
    return false;
}

bool DBusInstanceAvailabilityStatusChangedEvent::removeInterface(
        const std::string &_dbusObjectPath,
        const std::string &_dbusInterfaceName) {
    std::lock_guard<std::mutex> lock(interfacesMutex_);
    auto it = interfaces_.find(_dbusObjectPath);
    if(it != interfaces_.end()) {
        if(it->second.erase(_dbusInterfaceName) > 0)
            return true;
    }
    return false;
}

void DBusInstanceAvailabilityStatusChangedEvent::serviceInstancesAsyncCallback(
        std::shared_ptr<Proxy> _proxy,
        const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict _dict,
        GetAvailableServiceInstancesCallback &_call,
        std::shared_ptr<std::promise<CallStatus> > &_promise) {
    (void)_proxy;
    std::vector<DBusAddress> result;
    translate(_dict, result);
    _call(CallStatus::SUCCESS, result);
    _promise->set_value(CallStatus::SUCCESS);
}

void DBusInstanceAvailabilityStatusChangedEvent::translate(
        const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict &_dict,
        std::vector<DBusAddress> &_serviceInstances) {
    DBusAddress itsDBusAddress;

    const std::string &itsService = proxy_.getDBusAddress().getService();
    itsDBusAddress.setService(itsService);

    for (const auto &objectPathIter : _dict) {
        itsDBusAddress.setObjectPath(objectPathIter.first);

        const auto &interfacesDict = objectPathIter.second;
        for (const auto &interfaceIter : interfacesDict) {

            if (interfaceIter.first == observedDbusInterfaceName_) {
                itsDBusAddress.setInterface(interfaceIter.first);
                _serviceInstances.push_back(itsDBusAddress);
            }
        }
    }
}

} // namespace DBus
} // namespace CommonAPI


