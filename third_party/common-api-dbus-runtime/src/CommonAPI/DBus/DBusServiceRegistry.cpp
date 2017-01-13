// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iterator>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/DBus/DBusAddressTranslator.hpp>
#include <CommonAPI/DBus/DBusDaemonProxy.hpp>
#include <CommonAPI/DBus/DBusFunctionalHash.hpp>
#include <CommonAPI/DBus/DBusProxyAsyncCallbackHandler.hpp>
#include <CommonAPI/DBus/DBusServiceRegistry.hpp>
#include <CommonAPI/DBus/DBusTypes.hpp>
#include <CommonAPI/DBus/DBusUtils.hpp>

namespace CommonAPI {
namespace DBus {

DBusServiceRegistry::RegistryMap_t DBusServiceRegistry::registries_;
std::mutex DBusServiceRegistry::registriesMutex_;
static CommonAPI::CallInfo serviceRegistryInfo(10000);

std::shared_ptr<DBusServiceRegistry>
DBusServiceRegistry::get(std::shared_ptr<DBusProxyConnection> _connection) {
    std::lock_guard<std::mutex> itsGuard(registriesMutex_);
    auto registryIterator = registries_.find(_connection);
    if (registryIterator != registries_.end())
        return registryIterator->second;

    std::shared_ptr<DBusServiceRegistry> registry
        = std::make_shared<DBusServiceRegistry>(_connection);
    if (registry) {
        registry->init();
        registries_.insert( { _connection, registry } );
    }
    return registry;
}

DBusServiceRegistry::DBusServiceRegistry(std::shared_ptr<DBusProxyConnection> dbusProxyConnection) :
                dbusDaemonProxy_(std::make_shared<CommonAPI::DBus::DBusDaemonProxy>(dbusProxyConnection)),
                initialized_(false),
                servicesToResolve(0),
                objectPathsToResolve(0),
                notificationThread_() {
}

DBusServiceRegistry::~DBusServiceRegistry() {
    if (!initialized_) {
        return;
    }

    dbusDaemonProxy_->getNameOwnerChangedEvent().unsubscribe(dbusDaemonProxyNameOwnerChangedEventSubscription_);
    dbusDaemonProxy_->getProxyStatusEvent().unsubscribe(dbusDaemonProxyStatusEventSubscription_);

    // notify only listeners of resolved services (online > offline)
    for (auto& dbusServiceListenersIterator : dbusServiceListenersMap) {
        auto& dbusServiceListenersRecord = dbusServiceListenersIterator.second;

        if (dbusServiceListenersRecord.uniqueBusNameState == DBusRecordState::RESOLVED) {
            onDBusServiceNotAvailable(dbusServiceListenersRecord);
        }
    }

    // remove all object manager signal member handlers
    for (auto& dbusServiceUniqueNameIterator : dbusUniqueNamesMap_) {
        const auto& dbusServiceUniqueName = dbusServiceUniqueNameIterator.first;

        auto dbusProxyConnection = dbusDaemonProxy_->getDBusConnection();
        const bool isSubscriptionCancelled = dbusProxyConnection->removeObjectManagerSignalMemberHandler(
            dbusServiceUniqueName,
            this);
        assert(isSubscriptionCancelled);
        (void)isSubscriptionCancelled;
    }
}

void DBusServiceRegistry::init() {
    translator_ = DBusAddressTranslator::get();

    dbusDaemonProxyStatusEventSubscription_ =
                    dbusDaemonProxy_->getProxyStatusEvent().subscribe(
                        std::bind(&DBusServiceRegistry::onDBusDaemonProxyStatusEvent, this, std::placeholders::_1));

    dbusDaemonProxyNameOwnerChangedEventSubscription_ =
                    dbusDaemonProxy_->getNameOwnerChangedEvent().subscribe(
                        std::bind(&DBusServiceRegistry::onDBusDaemonProxyNameOwnerChangedEvent,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2,
                            std::placeholders::_3));

    fetchAllServiceNames(); // initialize list of registered bus names

    initialized_ = true;
}

DBusServiceRegistry::DBusServiceSubscription
DBusServiceRegistry::subscribeAvailabilityListener(
        const std::string &_address, DBusServiceListener serviceListener) {
    DBusAddress dbusAddress;
    translator_->translate(_address, dbusAddress);

    if (notificationThread_ == std::this_thread::get_id()) {
        COMMONAPI_ERROR(
            "You must not build proxies in callbacks of ProxyStatusEvent.",
            " Please refer to the documentation for suggestions how to avoid this.");
        assert(false);
    }

    std::lock_guard<std::mutex> dbusServicesLock(dbusServicesMutex_);
    auto& dbusServiceListenersRecord = dbusServiceListenersMap[dbusAddress.getService()];
    assert(dbusServiceListenersRecord.uniqueBusNameState != DBusRecordState::AVAILABLE);

    auto& dbusInterfaceNameListenersMap = dbusServiceListenersRecord.dbusObjectPathListenersMap[dbusAddress.getObjectPath()];
    auto& dbusInterfaceNameListenersRecord = dbusInterfaceNameListenersMap[dbusAddress.getInterface()];

    AvailabilityStatus availabilityStatus = AvailabilityStatus::UNKNOWN;

    if (dbusServiceListenersRecord.uniqueBusNameState == DBusRecordState::UNKNOWN) {
        dbusInterfaceNameListenersRecord.state = DBusRecordState::UNKNOWN;
        if (dbusServiceListenersRecord.uniqueBusNameState == DBusRecordState::UNKNOWN) {
            resolveDBusServiceName(dbusAddress.getService(), dbusServiceListenersRecord);
        }
    } else if (dbusServiceListenersRecord.uniqueBusNameState == DBusRecordState::NOT_AVAILABLE) {
        availabilityStatus = AvailabilityStatus::NOT_AVAILABLE;
    } else if (dbusServiceListenersRecord.uniqueBusNameState != DBusRecordState::RESOLVING &&
               dbusInterfaceNameListenersRecord.state == DBusRecordState::UNKNOWN) {
        if(dbusPredefinedServices_.find(dbusAddress.getService()) != dbusPredefinedServices_.end()) {
            //service is predefined -> notify service listeners about availability
            auto dbusServiceNameMapIterator = dbusServiceNameMap_.find(dbusAddress.getService());
            if(dbusServiceNameMapIterator != dbusServiceNameMap_.end()) {
                std::unordered_set<std::string> dbusInterfaceNames;
                for(auto dbusInterfaceNameListenerRecordIterator = dbusInterfaceNameListenersMap.begin();
                        dbusInterfaceNameListenerRecordIterator != dbusInterfaceNameListenersMap.end();
                        ++dbusInterfaceNameListenerRecordIterator) {
                    dbusInterfaceNames.insert(dbusInterfaceNameListenerRecordIterator->first);
                }
                notifyDBusServiceListeners(*dbusServiceNameMapIterator->second, dbusAddress.getObjectPath(), dbusInterfaceNames, DBusRecordState::AVAILABLE);
            }
        } else {
            dbusInterfaceNameListenersRecord.state = resolveDBusInterfaceNameState(dbusAddress, dbusServiceListenersRecord);
        }
    }

    if(availabilityStatus == AvailabilityStatus::UNKNOWN) {
        switch (dbusInterfaceNameListenersRecord.state) {
            case DBusRecordState::AVAILABLE:
                availabilityStatus = AvailabilityStatus::AVAILABLE;
                break;
            case DBusRecordState::NOT_AVAILABLE:
                availabilityStatus = AvailabilityStatus::NOT_AVAILABLE;
                break;
            default:
                availabilityStatus = AvailabilityStatus::UNKNOWN;
                break;
        }
    }

    // LB TODO: check this as it looks STRANGE!!!
    if (availabilityStatus != AvailabilityStatus::UNKNOWN) {
        notificationThread_ = std::this_thread::get_id();
        serviceListener(availabilityStatus);
        notificationThread_ = std::thread::id();
    }

    dbusInterfaceNameListenersRecord.listenerList.push_front(std::move(serviceListener));

    return dbusInterfaceNameListenersRecord.listenerList.begin();
}

void
DBusServiceRegistry::unsubscribeAvailabilityListener(
    const std::string &_address, DBusServiceSubscription& listenerSubscription) {
    DBusAddress dbusAddress;
    translator_->translate(_address, dbusAddress);

    std::lock_guard<std::mutex> dbusServicesLock(dbusServicesMutex_);
    auto dbusServiceListenersIterator = dbusServiceListenersMap.find(dbusAddress.getService());
    const bool isDBusServiceListenersRecordFound = (dbusServiceListenersIterator != dbusServiceListenersMap.end());

    if (!isDBusServiceListenersRecordFound) {
        return; // already unsubscribed
    }

    auto& dbusServiceListenersRecord = dbusServiceListenersIterator->second;
    auto dbusObjectPathListenersIterator =
                    dbusServiceListenersRecord.dbusObjectPathListenersMap.find(dbusAddress.getObjectPath());
    const bool isDBusObjectPathListenersRecordFound =
                    (dbusObjectPathListenersIterator != dbusServiceListenersRecord.dbusObjectPathListenersMap.end());

    if (!isDBusObjectPathListenersRecordFound) {
        return; // already unsubscribed
    }

    auto& dbusInterfaceNameListenersMap = dbusObjectPathListenersIterator->second;
    auto dbusInterfaceNameListenersIterator = dbusInterfaceNameListenersMap.find(dbusAddress.getInterface());
    const bool isDBusInterfaceNameListenersRecordFound =
                    (dbusInterfaceNameListenersIterator != dbusInterfaceNameListenersMap.end());

    if (!isDBusInterfaceNameListenersRecordFound) {
        return; // already unsubscribed
    }

    auto& dbusInterfaceNameListenersRecord = dbusInterfaceNameListenersIterator->second;

    dbusInterfaceNameListenersRecord.listenerList.erase(listenerSubscription);

    if (dbusInterfaceNameListenersRecord.listenerList.empty()) {
        dbusInterfaceNameListenersMap.erase(dbusInterfaceNameListenersIterator);

        if (dbusInterfaceNameListenersMap.empty()) {
            dbusServiceListenersRecord.dbusObjectPathListenersMap.erase(dbusObjectPathListenersIterator);
        }
    }
}

// d-feet mode until service is found
bool DBusServiceRegistry::isServiceInstanceAlive(const std::string& dbusInterfaceName,
                                                 const std::string& dbusServiceName,
                                                 const std::string& dbusObjectPath) {
    std::chrono::milliseconds timeout(1000);
    bool uniqueNameFound = false;

    DBusUniqueNameRecord* dbusUniqueNameRecord = NULL;
    std::string uniqueName;

    dbusServicesMutex_.lock();

    findCachedDbusService(dbusServiceName, &dbusUniqueNameRecord);

    if (dbusUniqueNameRecord != NULL) {
        uniqueName = dbusUniqueNameRecord->uniqueName;
        uniqueNameFound = true;
    }

    if (!uniqueNameFound) {
        DBusServiceListenersRecord dbusServiceListenersRecord;
        dbusServiceListenersRecord.uniqueBusNameState = DBusRecordState::RESOLVING;

        dbusServiceListenersRecord.futureOnResolve = dbusServiceListenersRecord.promiseOnResolve.get_future();
        std::unordered_map<std::string, DBusServiceListenersRecord>::value_type value(dbusServiceName, std::move(dbusServiceListenersRecord));
        auto insertedDbusServiceListenerRecord = dbusServiceListenersMap.insert(std::move(value));

        // start resolving only if dbusServiceListenerRecord was inserted, i.e. it is a new service that appeared
        if (insertedDbusServiceListenerRecord.second) {
            resolveDBusServiceName(dbusServiceName, dbusServiceListenersMap[dbusServiceName]);
        }

        dbusServicesMutex_.unlock();

        std::shared_future<DBusRecordState> futureNameResolved = insertedDbusServiceListenerRecord.first->second.futureOnResolve;
        futureNameResolved.wait_for(timeout);

        if(futureNameResolved.get() != DBusRecordState::RESOLVED) {
            return false;
        }

        dbusServicesMutex_.lock();
        auto dbusServiceListenersMapIterator = dbusServiceListenersMap.find(dbusServiceName);

        if(dbusServiceListenersMapIterator == dbusServiceListenersMap.end()) {
            dbusServicesMutex_.unlock();
            return false;
        }

        uniqueName = dbusServiceListenersMapIterator->second.uniqueBusName;

        if(uniqueName.empty() || dbusServiceListenersMapIterator->second.uniqueBusNameState != DBusRecordState::RESOLVED) {
            dbusServicesMutex_.unlock();
            return false;
        }

        auto dbusUniqueNameRecordIterator = dbusUniqueNamesMap_.find(uniqueName);

        if(dbusUniqueNameRecordIterator == dbusUniqueNamesMap_.end()) {
            dbusServicesMutex_.unlock();
            return false;
        }

        dbusUniqueNameRecord = &dbusUniqueNameRecordIterator->second;
    }

    dbusServicesMutex_.unlock();

    assert(dbusUniqueNameRecord != NULL);

    auto& dbusObjectPathsCache = dbusUniqueNameRecord->dbusObjectPathsCache;
    auto dbusObjectPathCacheIterator = dbusObjectPathsCache.find(dbusObjectPath);

    DBusObjectPathCache* dbusObjectPathCache = NULL;

    if(dbusObjectPathCacheIterator != dbusObjectPathsCache.end()) {
        dbusObjectPathCache = &(dbusObjectPathCacheIterator->second);
        if (dbusObjectPathCache->state != DBusRecordState::RESOLVED) {
            dbusObjectPathCache->state = DBusRecordState::RESOLVING;
            dbusServicesMutex_.lock();

            dbusObjectPathCache = &(dbusObjectPathCacheIterator->second);

            std::future<DBusRecordState> futureObjectPathResolved = dbusObjectPathCache->promiseOnResolve.get_future();
            dbusServicesMutex_.unlock();

            introspectDBusObjectPath(uniqueName, dbusObjectPath);
            futureObjectPathResolved.wait_for(timeout);
        }
    }
    else {
        // try to resolve object paths
        DBusObjectPathCache newDbusObjectPathCache;
        newDbusObjectPathCache.state = DBusRecordState::RESOLVING;
        newDbusObjectPathCache.serviceName = dbusServiceName;

        dbusServicesMutex_.lock();

        dbusObjectPathsCache.insert(std::make_pair(dbusObjectPath, std::move(newDbusObjectPathCache)));

        dbusObjectPathCacheIterator = dbusObjectPathsCache.find(dbusObjectPath);

        dbusObjectPathCache = &(dbusObjectPathCacheIterator->second);

        std::future<DBusRecordState> futureObjectPathResolved = dbusObjectPathCache->promiseOnResolve.get_future();
        dbusServicesMutex_.unlock();

        introspectDBusObjectPath(uniqueName, dbusObjectPath);
        futureObjectPathResolved.wait_for(timeout);
    }

    assert(dbusObjectPathCache != NULL);

    dbusServicesMutex_.lock();
    if(dbusObjectPathCache->state != DBusRecordState::RESOLVED) {
        dbusServicesMutex_.unlock();
        return false;
    }

    auto dbusInterfaceNamesIterator = dbusObjectPathCache->dbusInterfaceNamesCache.find(dbusInterfaceName);
    bool result = dbusInterfaceNamesIterator != dbusObjectPathCache->dbusInterfaceNamesCache.end();
    dbusServicesMutex_.unlock();

    return(result);
}

void DBusServiceRegistry::fetchAllServiceNames() {
    if (!dbusDaemonProxy_->isAvailable()) {
        return;
    }

    CallStatus callStatus;
    std::vector<std::string> availableServiceNames;

    dbusDaemonProxy_->listNames(callStatus, availableServiceNames);

    if (callStatus == CallStatus::SUCCESS) {
        for(std::string serviceName : availableServiceNames) {
            if(isDBusServiceName(serviceName)) {
                dbusServiceNameMap_[serviceName];
            }
        }
    }
}

// d-feet mode
std::vector<std::string> DBusServiceRegistry::getAvailableServiceInstances(const std::string& interfaceName,
                                                                           const std::string& domainName) {
    (void)domainName;
    std::vector<std::string> availableServiceInstances;

    // resolve all service names
    for (auto serviceNameIterator = dbusServiceNameMap_.begin();
                    serviceNameIterator != dbusServiceNameMap_.end();
                    serviceNameIterator++) {

        std::string serviceName = serviceNameIterator->first;
        DBusUniqueNameRecord* dbusUniqueNameRecord = serviceNameIterator->second;

        if(dbusUniqueNameRecord == NULL) {
            DBusServiceListenersRecord& serviceListenerRecord = dbusServiceListenersMap[serviceName];
            if(serviceListenerRecord.uniqueBusNameState != DBusRecordState::RESOLVING) {
                resolveDBusServiceName(serviceName, serviceListenerRecord);
            }
        }
    }

    std::mutex mutexResolveAllServices;
    std::unique_lock<std::mutex> lockResolveAllServices(mutexResolveAllServices);
    std::chrono::milliseconds timeout(5000);

    monitorResolveAllServices_.wait_for(lockResolveAllServices, timeout, [&] {
        mutexServiceResolveCount.lock();
        bool finished = servicesToResolve == 0;
        mutexServiceResolveCount.unlock();

        return finished;
    });

    for (auto serviceNameIterator = dbusServiceNameMap_.begin();
                    serviceNameIterator != dbusServiceNameMap_.end();
                    serviceNameIterator++) {

        std::string serviceName = serviceNameIterator->first;
        DBusUniqueNameRecord* dbusUniqueNameRecord = serviceNameIterator->second;

        if(dbusUniqueNameRecord != NULL) {
            if(dbusUniqueNameRecord->objectPathsState == DBusRecordState::UNKNOWN) {
                DBusObjectPathCache& rootObjectPathCache = dbusUniqueNameRecord->dbusObjectPathsCache["/"];
                if(rootObjectPathCache.state == DBusRecordState::UNKNOWN) {
                    rootObjectPathCache.state = DBusRecordState::RESOLVING;
                    introspectDBusObjectPath(dbusUniqueNameRecord->uniqueName, "/");
                }
            }
        }
    }

    std::mutex mutexResolveAllObjectPaths;
    std::unique_lock<std::mutex> lockResolveAllObjectPaths(mutexResolveAllObjectPaths);

    // TODO: Check if should use the remaining timeout not "used" during wait before
    monitorResolveAllObjectPaths_.wait_for(lockResolveAllObjectPaths, timeout, [&] {
        mutexServiceResolveCount.lock();
        bool finished = objectPathsToResolve == 0;
        mutexServiceResolveCount.unlock();

        return finished;
    });

    for (auto serviceNameIterator = dbusServiceNameMap_.begin();
                    serviceNameIterator != dbusServiceNameMap_.end();
                    serviceNameIterator++) {

        std::string serviceName = serviceNameIterator->first;
        DBusUniqueNameRecord* dbusUniqueNameRecord = serviceNameIterator->second;

        if(dbusUniqueNameRecord != NULL) {
            if(dbusUniqueNameRecord->objectPathsState == DBusRecordState::RESOLVED) {
                for (auto dbusObjectPathCacheIterator = dbusUniqueNameRecord->dbusObjectPathsCache.begin();
                                dbusObjectPathCacheIterator != dbusUniqueNameRecord->dbusObjectPathsCache.end();
                                dbusObjectPathCacheIterator++) {
                    if (dbusObjectPathCacheIterator->second.state == DBusRecordState::RESOLVED) {
                        if (dbusObjectPathCacheIterator->second.dbusInterfaceNamesCache.find(interfaceName)
                                        != dbusObjectPathCacheIterator->second.dbusInterfaceNamesCache.end()) {
                            std::string commonApiAddress;
                            translator_->translate(
                                    dbusObjectPathCacheIterator->first + "/" + interfaceName + "/" + serviceName, commonApiAddress);
                            availableServiceInstances.push_back(commonApiAddress);
                        }
                    }
                }
            }
        }
    }

    // maybe partial list but it contains everything we know for now
    return availableServiceInstances;
}

void DBusServiceRegistry::getAvailableServiceInstancesAsync(CommonAPI::Factory::AvailableInstancesCbk_t _cbk,
                                                            const std::string &_interface,
                                                            const std::string &_domain) {
    //Necessary as service discovery might need some time, but the async version of "getAvailableServiceInstances"
    //shall return without delay.
    std::thread(
            [this, _cbk, _interface, _domain](std::shared_ptr<DBusServiceRegistry> selfRef) {
                (void)selfRef;
                auto instances = getAvailableServiceInstances(_interface, _domain);
                _cbk(instances);
            }, this->shared_from_this()
    ).detach();
}

void DBusServiceRegistry::onSignalDBusMessage(const DBusMessage &_dbusMessage) {
    const std::string& dbusServiceUniqueName = _dbusMessage.getSender();

    assert(_dbusMessage.isSignalType());
    assert(_dbusMessage.hasInterfaceName("org.freedesktop.DBus.ObjectManager"));
    assert(_dbusMessage.hasMemberName("InterfacesAdded") || _dbusMessage.hasMemberName("InterfacesRemoved"));

    DBusInputStream dbusInputStream(_dbusMessage);
    std::string dbusObjectPath;
    std::unordered_set<std::string> dbusInterfaceNames;
    DBusRecordState dbusInterfaceNameState;

    dbusInputStream >> dbusObjectPath;

    if (_dbusMessage.hasMemberName("InterfacesAdded")) {
        std::string dbusInterfaceName;
        dbusInterfaceNameState = DBusRecordState::AVAILABLE;

        dbusInputStream.beginReadMapOfSerializableStructs();
        while (!dbusInputStream.readMapCompleted()) {
            dbusInputStream.align(8);
            dbusInputStream >> dbusInterfaceName;
            dbusInputStream.skipMap();
            assert(!dbusInputStream.hasError());
            dbusInterfaceNames.insert(dbusInterfaceName);
        }
        dbusInputStream.endReadMapOfSerializableStructs();
    } else {
        std::vector<std::string> removedDBusInterfaceNames;

        dbusInterfaceNameState = DBusRecordState::NOT_AVAILABLE;
        dbusInputStream >> removedDBusInterfaceNames;

        std::move(
            removedDBusInterfaceNames.begin(),
            removedDBusInterfaceNames.end(),
            std::inserter(dbusInterfaceNames, dbusInterfaceNames.begin()));
    }

    if (dbusInputStream.hasError()) {
        return;
    }

    if (dbusInterfaceNames.empty()) {
        return;
    }

    std::lock_guard<std::mutex> dbusServicesLock(dbusServicesMutex_);

    auto dbusServiceUniqueNameIterator = dbusUniqueNamesMap_.find(dbusServiceUniqueName);
    const bool isDBusServiceUniqueNameFound = (dbusServiceUniqueNameIterator != dbusUniqueNamesMap_.end());

    if (!isDBusServiceUniqueNameFound) {
        // LB TODO: unsubscribe here!
        // Needs to be reworked in order to store the subscription identifier!
        return;
    }

    auto& dbusUniqueNameRecord = dbusServiceUniqueNameIterator->second;

    DBusObjectPathCache *dbusObjectPathRecord;
    auto dbusObjectPathCacheIterator = dbusUniqueNameRecord.dbusObjectPathsCache.find(dbusObjectPath);
    if(dbusObjectPathCacheIterator != dbusUniqueNameRecord.dbusObjectPathsCache.end())
        dbusObjectPathRecord = &(dbusObjectPathCacheIterator->second);
    else
        return;

    if (dbusObjectPathRecord->state != DBusRecordState::RESOLVED) {
        return;
    }

    for (const auto& dbusInterfaceName : dbusInterfaceNames) {
        if (dbusInterfaceNameState == DBusRecordState::AVAILABLE) {
            dbusObjectPathRecord->dbusInterfaceNamesCache.insert(dbusInterfaceName);
        } else {
            dbusObjectPathRecord->dbusInterfaceNamesCache.erase(dbusInterfaceName);
        }
    }

    notifyDBusServiceListeners(dbusUniqueNameRecord, dbusObjectPath, dbusInterfaceNames, dbusInterfaceNameState);
}

void DBusServiceRegistry::setDBusServicePredefined(const std::string& _serviceName) {
    dbusPredefinedServices_.insert(_serviceName);
}

void DBusServiceRegistry::resolveDBusServiceName(const std::string& dbusServiceName,
                                                 DBusServiceListenersRecord& dbusServiceListenersRecord) {
    assert(dbusServiceListenersRecord.uniqueBusNameState != DBusRecordState::RESOLVED);
    assert(dbusServiceListenersRecord.uniqueBusName.empty());

    mutexServiceResolveCount.lock();
    servicesToResolve++;
    mutexServiceResolveCount.unlock();

    if (dbusDaemonProxy_->isAvailable()) {
        dbusDaemonProxy_->getNameOwnerAsync(
            dbusServiceName,
            std::bind(
                &DBusServiceRegistry::onGetNameOwnerCallback,
                this->shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2,
                dbusServiceName));

        dbusServiceListenersRecord.uniqueBusNameState = DBusRecordState::RESOLVING;
    }
}

void DBusServiceRegistry::onGetNameOwnerCallback(const CallStatus& status,
                                                 std::string dbusServiceUniqueName,
                                                 const std::string& dbusServiceName) {
    std::lock_guard<std::mutex> dbusServicesLock(dbusServicesMutex_);

    auto dbusServiceListenerIterator = dbusServiceListenersMap.find(dbusServiceName);
    const bool isDBusServiceListenerRecordFound = (dbusServiceListenerIterator != dbusServiceListenersMap.end());

    if (!isDBusServiceListenerRecordFound) {
        return;
    }

    DBusServiceListenersRecord& dbusServiceListenersRecord = dbusServiceListenerIterator->second;

    if (status == CallStatus::SUCCESS) {
        onDBusServiceAvailable(dbusServiceName, dbusServiceUniqueName);
        if(dbusServiceListenersRecord.futureOnResolve.valid()) {
            dbusServiceListenersRecord.promiseOnResolve.set_value(DBusRecordState(dbusServiceListenersRecord.uniqueBusNameState));
        }
    } else {
        // try to fulfill open promises
        if(dbusServiceListenersRecord.futureOnResolve.valid()) {
            dbusServiceListenersRecord.promiseOnResolve.set_value(DBusRecordState::NOT_AVAILABLE);
        }

        onDBusServiceNotAvailable(dbusServiceListenersRecord, dbusServiceName);
    }

    mutexServiceResolveCount.lock();
    servicesToResolve--;
    mutexServiceResolveCount.unlock();
    monitorResolveAllServices_.notify_all();
}

DBusServiceRegistry::DBusRecordState
DBusServiceRegistry::resolveDBusInterfaceNameState(
    const DBusAddress &_dbusAddress, DBusServiceListenersRecord &dbusServiceListenersRecord) {
    assert(dbusServiceListenersRecord.uniqueBusNameState == DBusRecordState::RESOLVED);
    assert(!dbusServiceListenersRecord.uniqueBusName.empty());

    auto& dbusServiceUniqueNameRecord = dbusUniqueNamesMap_[dbusServiceListenersRecord.uniqueBusName];
    assert(!dbusServiceUniqueNameRecord.ownedBusNames.empty());

    auto& dbusObjectPathRecord = getDBusObjectPathCacheReference(
        _dbusAddress.getObjectPath(),
        _dbusAddress.getService(),
        dbusServiceListenersRecord.uniqueBusName,
        dbusServiceUniqueNameRecord);

    if (dbusObjectPathRecord.state != DBusRecordState::RESOLVED) {
        return dbusObjectPathRecord.state;
    }

    auto dbusInterfaceNameIterator
        = dbusObjectPathRecord.dbusInterfaceNamesCache.find(_dbusAddress.getInterface());
    const bool isDBusInterfaceNameFound =
                    (dbusInterfaceNameIterator != dbusObjectPathRecord.dbusInterfaceNamesCache.end());

    return isDBusInterfaceNameFound ? DBusRecordState::AVAILABLE : DBusRecordState::NOT_AVAILABLE;
}


DBusServiceRegistry::DBusObjectPathCache &
DBusServiceRegistry::getDBusObjectPathCacheReference(
        const std::string& dbusObjectPath,
        const std::string& dbusServiceName,
        const std::string& dbusServiceUniqueName,
        DBusUniqueNameRecord& dbusUniqueNameRecord) {
    const bool isFirstDBusObjectPathCache = dbusUniqueNameRecord.dbusObjectPathsCache.empty();

    auto dbusObjectPathCacheIterator = dbusUniqueNameRecord.dbusObjectPathsCache.find(dbusObjectPath);
    if(dbusObjectPathCacheIterator == dbusUniqueNameRecord.dbusObjectPathsCache.end()) {
        DBusObjectPathCache objectPathCache;
        objectPathCache.serviceName = dbusServiceName;
        std::unordered_map<std::string, DBusObjectPathCache>::value_type value (dbusObjectPath, std::move(objectPathCache));
        dbusUniqueNameRecord.dbusObjectPathsCache.insert(std::move(value));
        dbusObjectPathCacheIterator = dbusUniqueNameRecord.dbusObjectPathsCache.find(dbusObjectPath);
    }

    if (isFirstDBusObjectPathCache) {
        auto dbusProxyConnection = dbusDaemonProxy_->getDBusConnection();
        const bool isSubscriptionSuccessful = dbusProxyConnection->addObjectManagerSignalMemberHandler(
            dbusServiceUniqueName,
            this);
        assert(isSubscriptionSuccessful);
        (void)isSubscriptionSuccessful;
    }

    if (dbusObjectPathCacheIterator->second.state == DBusRecordState::UNKNOWN
                    && resolveObjectPathWithObjectManager(dbusServiceUniqueName, dbusObjectPath)) {
        dbusObjectPathCacheIterator->second.state = DBusRecordState::RESOLVING;
    }

    return dbusObjectPathCacheIterator->second;
}

void DBusServiceRegistry::releaseDBusObjectPathCacheReference(const std::string& dbusObjectPath,
                                                              const DBusServiceListenersRecord& dbusServiceListenersRecord) {
    if (!dbusDaemonProxy_->isAvailable()) {
        return;
    }

    if (dbusServiceListenersRecord.uniqueBusNameState != DBusRecordState::RESOLVED) {
        return;
    }

    assert(!dbusServiceListenersRecord.uniqueBusName.empty());

    auto& dbusUniqueNameRecord = dbusUniqueNamesMap_[dbusServiceListenersRecord.uniqueBusName];
    assert(!dbusUniqueNameRecord.ownedBusNames.empty());
    assert(!dbusUniqueNameRecord.dbusObjectPathsCache.empty());

    auto dbusObjectPathCacheIterator = dbusUniqueNameRecord.dbusObjectPathsCache.find(dbusObjectPath);
    const bool isDBusObjectPathCacheFound = (dbusObjectPathCacheIterator != dbusUniqueNameRecord.dbusObjectPathsCache.end());
    assert(isDBusObjectPathCacheFound);
    (void)isDBusObjectPathCacheFound;

    auto& dbusObjectPathCache = dbusObjectPathCacheIterator->second;
    assert(dbusObjectPathCache.referenceCount > 0);

    dbusObjectPathCache.referenceCount--;

    if (dbusObjectPathCache.referenceCount == 0) {
        dbusUniqueNameRecord.dbusObjectPathsCache.erase(dbusObjectPathCacheIterator);

        const bool isLastDBusObjectPathCache = dbusUniqueNameRecord.dbusObjectPathsCache.empty();
        if (isLastDBusObjectPathCache) {
            auto dbusProxyConnection = dbusDaemonProxy_->getDBusConnection();
            const bool isSubscriptionCancelled = dbusProxyConnection->removeObjectManagerSignalMemberHandler(
                dbusServiceListenersRecord.uniqueBusName,
                this);
            assert(isSubscriptionCancelled);
            (void)isSubscriptionCancelled;
        }
    }
}

bool DBusServiceRegistry::resolveObjectPathWithObjectManager(const std::string& dbusServiceUniqueName, const std::string& dbusObjectPath) {
    // get managed objects from root object manager
    auto getManagedObjectsCallback = std::bind(
            &DBusServiceRegistry::onGetManagedObjectsCallbackResolve,
            this->shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2,
            dbusServiceUniqueName,
            dbusObjectPath);
    return getManagedObjects(dbusServiceUniqueName, "/", getManagedObjectsCallback);
}

bool DBusServiceRegistry::getManagedObjects(const std::string& dbusServiceUniqueName,
                                            const std::string& dbusObjectPath,
                                            GetManagedObjectsCallback callback) {
    bool isSendingInProgress = false;
    auto dbusConnection = dbusDaemonProxy_->getDBusConnection();

    assert(!dbusServiceUniqueName.empty());

    if(dbusConnection->isConnected()) {

        if(dbusObjectPath != "/") {
            mutexObjectPathsResolveCount.lock();
            objectPathsToResolve++;
            mutexObjectPathsResolveCount.unlock();
        }

        DBusAddress dbusAddress(dbusServiceUniqueName, dbusObjectPath, "org.freedesktop.DBus.ObjectManager");
        DBusMessage dbusMessageCall = CommonAPI::DBus::DBusMessage::createMethodCall(
                dbusAddress,
                "GetManagedObjects");

        auto getManagedObjectsCallback = std::bind(
                callback,
                std::placeholders::_1,
                std::placeholders::_2,
                dbusServiceUniqueName,
                dbusObjectPath);

        dbusConnection->sendDBusMessageWithReplyAsync(
                dbusMessageCall,
                DBusProxyAsyncCallbackHandler<
                    DBusObjectManagerStub::DBusObjectPathAndInterfacesDict
                >::create(getManagedObjectsCallback, std::tuple<DBusObjectManagerStub::DBusObjectPathAndInterfacesDict>()),
                &serviceRegistryInfo);

        isSendingInProgress = true;
    }
    return isSendingInProgress;
}

void DBusServiceRegistry::onGetManagedObjectsCallbackResolve(const CallStatus& callStatus,
                                 const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict dbusObjectPathAndInterfacesDict,
                                 const std::string& dbusServiceUniqueName,
                                 const std::string& dbusObjectPath) {

    if(callStatus == CallStatus::SUCCESS) {
        //has object manager
        bool objectPathFound = false;
        for(auto objectPathDict : dbusObjectPathAndInterfacesDict)
        {
            std::string objectPath = objectPathDict.first;
            if(objectPath != dbusObjectPath)
                continue;

            // object path that should be resolved is found --> resolve
            objectPathFound = true;
            CommonAPI::DBus::DBusObjectManagerStub::DBusInterfacesAndPropertiesDict interfacesAndPropertiesDict = objectPathDict.second;
            for(auto interfaceDict : interfacesAndPropertiesDict)
            {
                std::string interfaceName = interfaceDict.first;
                dbusServicesMutex_.lock();
                processManagedObject(dbusObjectPath, dbusServiceUniqueName, interfaceName);
                dbusServicesMutex_.unlock();
            }

            // resolve further managed objects
            auto callback = std::bind(
                    &DBusServiceRegistry::onGetManagedObjectsCallbackResolveFurther,
                    this->shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3,
                    std::placeholders::_4);
            getManagedObjects(dbusServiceUniqueName, dbusObjectPath, callback);
        }

        if(!objectPathFound) {
            // object path is managed. Try to resolve object path with the help of the manager
            auto getManagedObjectsCallback = std::bind(
                    &DBusServiceRegistry::onGetManagedObjectsCallbackResolve,
                    this->shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2,
                    dbusServiceUniqueName,
                    dbusObjectPath);
            std::string objectPathManager = dbusObjectPath.substr(0, dbusObjectPath.find_last_of("\\/"));
            getManagedObjects(dbusServiceUniqueName, objectPathManager, getManagedObjectsCallback);
        }
    } else {
        COMMONAPI_ERROR("There is no Object Manager that manages " + dbusObjectPath + ". Resolving failed!");
    }
}

void DBusServiceRegistry::onGetManagedObjectsCallbackResolveFurther(const CallStatus& callStatus,
                                     const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict dbusObjectPathAndInterfacesDict,
                                     const std::string& dbusServiceUniqueName,
                                     const std::string& dbusObjectPath) {

    if(callStatus == CallStatus::SUCCESS) {
        for(auto objectPathDict : dbusObjectPathAndInterfacesDict)
        {
            //resolve
            std::string objectPath = objectPathDict.first;
            CommonAPI::DBus::DBusObjectManagerStub::DBusInterfacesAndPropertiesDict interfacesAndPropertiesDict = objectPathDict.second;
            for(auto interfaceDict : interfacesAndPropertiesDict)
            {
                std::string interfaceName = interfaceDict.first;
                dbusServicesMutex_.lock();
                processManagedObject(objectPath, dbusServiceUniqueName, interfaceName);
                dbusServicesMutex_.unlock();
            }

            // resolve further managed objects
            auto callback = std::bind(
                    &DBusServiceRegistry::onGetManagedObjectsCallbackResolveFurther,
                    this->shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3,
                    std::placeholders::_4);
            getManagedObjects(dbusServiceUniqueName, objectPath, callback);
        }
    } else {
        // No further managed objects
    }

    std::lock_guard<std::mutex> dbusServicesLock(dbusServicesMutex_);

    auto dbusServiceUniqueNameIterator = dbusUniqueNamesMap_.find(dbusServiceUniqueName);
    const bool isDBusServiceUniqueNameFound = (dbusServiceUniqueNameIterator != dbusUniqueNamesMap_.end());

    if (!isDBusServiceUniqueNameFound) {
        return;
    }

    DBusUniqueNameRecord& dbusUniqueNameRecord = dbusServiceUniqueNameIterator->second;
    auto dbusObjectPathIterator = dbusUniqueNameRecord.dbusObjectPathsCache.find(dbusObjectPath);
    const bool isDBusObjectPathFound = (dbusObjectPathIterator != dbusUniqueNameRecord.dbusObjectPathsCache.end());

    if (!isDBusObjectPathFound) {
        return;
    }

    DBusObjectPathCache& dbusObjectPathRecord = dbusObjectPathIterator->second;

    dbusObjectPathRecord.state = DBusRecordState::RESOLVED;
    dbusObjectPathRecord.promiseOnResolve.set_value(dbusObjectPathRecord.state);
    mutexObjectPathsResolveCount.lock();
    objectPathsToResolve--;
    mutexObjectPathsResolveCount.unlock();
    monitorResolveAllObjectPaths_.notify_all();

    dbusUniqueNameRecord.objectPathsState = DBusRecordState::RESOLVED;

    notifyDBusServiceListeners(
        dbusUniqueNameRecord,
        dbusObjectPath,
        dbusObjectPathRecord.dbusInterfaceNamesCache,
        DBusRecordState::RESOLVED);
}

void DBusServiceRegistry::processManagedObject(const std::string& dbusObjectPath,
                                               const std::string& dbusServiceUniqueName,
                                               const std::string& interfaceName) {
    DBusUniqueNameRecord& dbusUniqueNameRecord = dbusUniqueNamesMap_[dbusServiceUniqueName];
    DBusObjectPathCache& dbusObjectPathCache = dbusUniqueNameRecord.dbusObjectPathsCache[dbusObjectPath];

    if(!isOrgFreedesktopDBusInterface(interfaceName)) {
        dbusObjectPathCache.dbusInterfaceNamesCache.insert(interfaceName);
    } else if (translator_->isOrgFreedesktopDBusPeerMapped() && (interfaceName == "org.freedesktop.DBus.Peer")) {
        dbusObjectPathCache.dbusInterfaceNamesCache.insert(interfaceName);
    }
}

bool DBusServiceRegistry::introspectDBusObjectPath(const std::string& dbusServiceUniqueName,
                                                   const std::string& dbusObjectPath) {
    bool isResolvingInProgress = false;
    auto dbusConnection = dbusDaemonProxy_->getDBusConnection();

    assert(!dbusServiceUniqueName.empty());

    if (dbusConnection->isConnected()) {
        mutexObjectPathsResolveCount.lock();
        objectPathsToResolve++;
        mutexObjectPathsResolveCount.unlock();

        DBusAddress dbusAddress(dbusServiceUniqueName, dbusObjectPath, "org.freedesktop.DBus.Introspectable");
        DBusMessage dbusMessageCall = DBusMessage::createMethodCall(
            dbusAddress,
            "Introspect");
        auto instrospectAsyncCallback = std::bind(
            &DBusServiceRegistry::onIntrospectCallback,
            this->shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2,
            dbusServiceUniqueName,
            dbusObjectPath);

        dbusConnection->sendDBusMessageWithReplyAsync(
            dbusMessageCall,
            DBusProxyAsyncCallbackHandler<
               std::string
            >::create(instrospectAsyncCallback, std::tuple<std::string>()),
            &serviceRegistryInfo);

        isResolvingInProgress = true;
    }
    return isResolvingInProgress;
}

/**
 * Callback for org.freedesktop.DBus.Introspectable.Introspect
 *
 * This is the other end of checking if a dbus object path is available.
 * On success it'll extract all interface names from the xml data response.
 * Special interfaces that start with "org.freedesktop.DBus." will be ignored.
 *
 * @param status
 * @param xmlData
 * @param dbusServiceUniqueName
 * @param dbusObjectPath
 */
void DBusServiceRegistry::onIntrospectCallback(const CallStatus& callStatus,
                                               std::string xmlData,
                                               const std::string& dbusServiceUniqueName,
                                               const std::string& dbusObjectPath) {
    if (callStatus == CallStatus::SUCCESS) {
        parseIntrospectionData(xmlData, dbusObjectPath, dbusServiceUniqueName);
    }

    std::lock_guard<std::mutex> dbusServicesLock(dbusServicesMutex_);

    // Error CallStatus will result in empty parsedDBusInterfaceNameSet (and not available notification)

    auto dbusServiceUniqueNameIterator = dbusUniqueNamesMap_.find(dbusServiceUniqueName);
    const bool isDBusServiceUniqueNameFound = (dbusServiceUniqueNameIterator != dbusUniqueNamesMap_.end());

    if (!isDBusServiceUniqueNameFound) {
        return;
    }

    DBusUniqueNameRecord& dbusUniqueNameRecord = dbusServiceUniqueNameIterator->second;
    auto dbusObjectPathIterator = dbusUniqueNameRecord.dbusObjectPathsCache.find(dbusObjectPath);
    const bool isDBusObjectPathFound = (dbusObjectPathIterator != dbusUniqueNameRecord.dbusObjectPathsCache.end());

    if (!isDBusObjectPathFound) {
        return;
    }

    DBusObjectPathCache& dbusObjectPathRecord = dbusObjectPathIterator->second;

    dbusObjectPathRecord.state = DBusRecordState::RESOLVED;
    dbusObjectPathRecord.promiseOnResolve.set_value(dbusObjectPathRecord.state);
    mutexObjectPathsResolveCount.lock();
    objectPathsToResolve--;
    mutexObjectPathsResolveCount.unlock();
    monitorResolveAllObjectPaths_.notify_all();

    dbusUniqueNameRecord.objectPathsState = DBusRecordState::RESOLVED;

    notifyDBusServiceListeners(
        dbusUniqueNameRecord,
        dbusObjectPath,
        dbusObjectPathRecord.dbusInterfaceNamesCache,
        DBusRecordState::RESOLVED);
}

void DBusServiceRegistry::parseIntrospectionNode(const pugi::xml_node& node, const std::string& rootObjectPath, const std::string& fullObjectPath, const std::string& dbusServiceUniqueName) {
    std::string nodeName;

    for(pugi::xml_node& subNode : node.children()) {
        nodeName = std::string(subNode.name());

        if(nodeName == "node") {
            processIntrospectionObjectPath(subNode, rootObjectPath, dbusServiceUniqueName);
        }

        if(nodeName == "interface") {
            processIntrospectionInterface(subNode, rootObjectPath, fullObjectPath, dbusServiceUniqueName);
        }
    }
}

void DBusServiceRegistry::processIntrospectionObjectPath(const pugi::xml_node& node, const std::string& rootObjectPath, const std::string& dbusServiceUniqueName) {
    std::string fullObjectPath = rootObjectPath;

    if(fullObjectPath.at(fullObjectPath.length()-1) != '/') {
        fullObjectPath += "/";
    }

    fullObjectPath += std::string(node.attribute("name").as_string());

    DBusUniqueNameRecord& dbusUniqueNameRecord = dbusUniqueNamesMap_[dbusServiceUniqueName];
    DBusObjectPathCache& dbusObjectPathCache = dbusUniqueNameRecord.dbusObjectPathsCache[fullObjectPath];

    if(dbusObjectPathCache.state == DBusRecordState::UNKNOWN) {
        dbusObjectPathCache.state = DBusRecordState::RESOLVING;
        introspectDBusObjectPath(dbusServiceUniqueName, fullObjectPath);
    }

    for(pugi::xml_node subNode : node.children()) {
        parseIntrospectionNode(subNode, fullObjectPath, fullObjectPath, dbusServiceUniqueName);
    }
}

void DBusServiceRegistry::processIntrospectionInterface(const pugi::xml_node& node, const std::string& rootObjectPath, const std::string& fullObjectPath, const std::string& dbusServiceUniqueName) {
    std::string interfaceName = node.attribute("name").as_string();
    DBusUniqueNameRecord& dbusUniqueNameRecord = dbusUniqueNamesMap_[dbusServiceUniqueName];
    DBusObjectPathCache& dbusObjectPathCache = dbusUniqueNameRecord.dbusObjectPathsCache[fullObjectPath];

    if(!isOrgFreedesktopDBusInterface(interfaceName)) {
        dbusObjectPathCache.dbusInterfaceNamesCache.insert(interfaceName);
    } else if (translator_->isOrgFreedesktopDBusPeerMapped() && (interfaceName == "org.freedesktop.DBus.Peer")) {
        dbusObjectPathCache.dbusInterfaceNamesCache.insert(interfaceName);
    }

    for(pugi::xml_node subNode : node.children()) {
        parseIntrospectionNode(subNode, rootObjectPath, fullObjectPath, dbusServiceUniqueName);
    }
}

void DBusServiceRegistry::parseIntrospectionData(const std::string& xmlData,
                                                 const std::string& rootObjectPath,
                                                 const std::string& dbusServiceUniqueName) {
    pugi::xml_document xmlDocument;
    pugi::xml_parse_result parsedResult = xmlDocument.load_buffer(xmlData.c_str(), xmlData.length(), pugi::parse_minimal, pugi::encoding_utf8);

    if(parsedResult.status != pugi::xml_parse_status::status_ok) {
        return;
    }

    const pugi::xml_node rootNode = xmlDocument.child("node");

    dbusServicesMutex_.lock();

    parseIntrospectionNode(rootNode, rootObjectPath, rootObjectPath, dbusServiceUniqueName);

    dbusUniqueNamesMap_[dbusServiceUniqueName];
    dbusServicesMutex_.unlock();
}


void DBusServiceRegistry::onDBusDaemonProxyStatusEvent(const AvailabilityStatus& availabilityStatus) {
    assert(availabilityStatus != AvailabilityStatus::UNKNOWN);

    std::lock_guard<std::mutex> dbusServicesLock(dbusServicesMutex_);

    for (auto& dbusServiceListenersIterator : dbusServiceListenersMap) {
        const auto& dbusServiceName = dbusServiceListenersIterator.first;
        auto& dbusServiceListenersRecord = dbusServiceListenersIterator.second;

        if (availabilityStatus == AvailabilityStatus::AVAILABLE) {
            resolveDBusServiceName(dbusServiceName, dbusServiceListenersRecord);
        } else {
            onDBusServiceNotAvailable(dbusServiceListenersRecord, dbusServiceName);
        }
    }
}

void DBusServiceRegistry::checkDBusServiceWasAvailable(const std::string& dbusServiceName,
                                                       const std::string& dbusServiceUniqueName) {
    auto dbusUniqueNameIterator = dbusUniqueNamesMap_.find(dbusServiceUniqueName);
    const bool isDBusUniqueNameFound = (dbusUniqueNameIterator != dbusUniqueNamesMap_.end());

    if (isDBusUniqueNameFound) {
        auto& dbusServiceListenersRecord = dbusServiceListenersMap[dbusServiceName];
        onDBusServiceNotAvailable(dbusServiceListenersRecord, dbusServiceName);
    }
}

void DBusServiceRegistry::onDBusDaemonProxyNameOwnerChangedEvent(const std::string& affectedName,
                                                                 const std::string& oldOwner,
                                                                 const std::string& newOwner) {
    if (!isDBusServiceName(affectedName)) {
        return;
    }
    const bool isDBusServiceNameLost = newOwner.empty();
    const std::string& dbusServiceUniqueName = (isDBusServiceNameLost ? oldOwner : newOwner);

    std::lock_guard<std::mutex> dbusServicesLock(dbusServicesMutex_);

    if (isDBusServiceNameLost) {
        checkDBusServiceWasAvailable(affectedName, dbusServiceUniqueName);
    } else {
        onDBusServiceAvailable(affectedName, dbusServiceUniqueName);
    }

    return;
}


void DBusServiceRegistry::onDBusServiceAvailable(const std::string& dbusServiceName,
                                                 const std::string& dbusServiceUniqueName) {
    DBusUniqueNameRecord* dbusUniqueNameRecord = insertServiceNameMapping(dbusServiceUniqueName, dbusServiceName);

    auto& dbusServiceListenersRecord = dbusServiceListenersMap[dbusServiceName];
    const bool isDBusServiceNameObserved = !dbusServiceListenersRecord.dbusObjectPathListenersMap.empty();

    if (dbusServiceListenersRecord.uniqueBusNameState == DBusRecordState::RESOLVED
                    && dbusServiceListenersRecord.uniqueBusName != dbusServiceUniqueName) {
        //A new unique connection name claims an already claimed name
        //-> release of old name and claim of new name arrive in reverted order.
        //Therefore: Release of old and proceed with claiming of new owner.
        checkDBusServiceWasAvailable(dbusServiceName, dbusServiceListenersRecord.uniqueBusName);
    }

    dbusServiceListenersRecord.uniqueBusNameState = DBusRecordState::RESOLVED;
    dbusServiceListenersRecord.uniqueBusName = dbusServiceUniqueName;

    if (!isDBusServiceNameObserved) {
        return;
    }
    for (auto dbusObjectPathListenersIterator = dbusServiceListenersRecord.dbusObjectPathListenersMap.begin();
                    dbusObjectPathListenersIterator != dbusServiceListenersRecord.dbusObjectPathListenersMap.end();) {
        const std::string& listenersDBusObjectPath = dbusObjectPathListenersIterator->first;
        auto& dbusInterfaceNameListenersMap = dbusObjectPathListenersIterator->second;

        if(dbusPredefinedServices_.find(dbusServiceName) == dbusPredefinedServices_.end()) {
            //service not predefined -> resolve object path and notify service listeners
            auto& dbusObjectPathRecord = getDBusObjectPathCacheReference(
                listenersDBusObjectPath,
                dbusServiceName,
                dbusServiceUniqueName,
                *dbusUniqueNameRecord);

            if (dbusObjectPathRecord.state == DBusRecordState::RESOLVED) {
                notifyDBusObjectPathResolved(dbusInterfaceNameListenersMap, dbusObjectPathRecord.dbusInterfaceNamesCache);
            }
        } else {
            //service is predefined -> notify service listeners about availability
            std::unordered_set<std::string> dbusInterfaceNames;
            for(auto dbusInterfaceNameListenerRecordIterator = dbusInterfaceNameListenersMap.begin();
                    dbusInterfaceNameListenerRecordIterator != dbusInterfaceNameListenersMap.end();
                    ++dbusInterfaceNameListenerRecordIterator) {
                dbusInterfaceNames.insert(dbusInterfaceNameListenerRecordIterator->first);
            }
            notifyDBusServiceListeners(*dbusUniqueNameRecord, listenersDBusObjectPath, dbusInterfaceNames, DBusRecordState::AVAILABLE);
        }

        if (dbusInterfaceNameListenersMap.empty()) {
            dbusObjectPathListenersIterator = dbusServiceListenersRecord.dbusObjectPathListenersMap.erase(
                dbusObjectPathListenersIterator);
        } else {
            dbusObjectPathListenersIterator++;
        }
    }
}

void DBusServiceRegistry::onDBusServiceNotAvailable(DBusServiceListenersRecord& dbusServiceListenersRecord, const std::string &_serviceName) {
    const std::unordered_set<std::string> dbusInterfaceNamesCache;

    const DBusUniqueNamesMapIterator dbusUniqueNameRecordIterator = dbusUniqueNamesMap_.find(dbusServiceListenersRecord.uniqueBusName);

    if (dbusUniqueNameRecordIterator != dbusUniqueNamesMap_.end()) {
        removeUniqueName(dbusUniqueNameRecordIterator, _serviceName);
    }

    dbusServiceListenersRecord.uniqueBusName.clear();
    dbusServiceListenersRecord.uniqueBusNameState = DBusRecordState::NOT_AVAILABLE;

    for (auto dbusObjectPathListenersIterator = dbusServiceListenersRecord.dbusObjectPathListenersMap.begin();
                    dbusObjectPathListenersIterator != dbusServiceListenersRecord.dbusObjectPathListenersMap.end(); ) {
        auto& dbusInterfaceNameListenersMap = dbusObjectPathListenersIterator->second;

        notifyDBusObjectPathResolved(dbusInterfaceNameListenersMap, dbusInterfaceNamesCache);

        if (dbusInterfaceNameListenersMap.empty()) {
            dbusObjectPathListenersIterator = dbusServiceListenersRecord.dbusObjectPathListenersMap.erase(
                dbusObjectPathListenersIterator);
        } else {
            dbusObjectPathListenersIterator++;
        }
    }
}

void DBusServiceRegistry::notifyDBusServiceListeners(const DBusUniqueNameRecord& dbusUniqueNameRecord,
                                                     const std::string& dbusObjectPath,
                                                     const std::unordered_set<std::string>& dbusInterfaceNames,
                                                     const DBusRecordState& dbusInterfaceNamesState) {
    notificationThread_ = std::this_thread::get_id();

    for (const std::string& dbusServiceName : dbusUniqueNameRecord.ownedBusNames) {
        auto dbusServiceListenersIterator = dbusServiceListenersMap.find(dbusServiceName);

        if(dbusServiceListenersIterator == dbusServiceListenersMap.end()) {
            continue;
        }

        DBusServiceListenersRecord& dbusServiceListenersRecord = dbusServiceListenersIterator->second;
        if(dbusServiceListenersRecord.uniqueBusNameState != DBusRecordState::RESOLVED) {
            continue;
        }

        auto dbusObjectPathListenersIterator = dbusServiceListenersRecord.dbusObjectPathListenersMap.find(dbusObjectPath);
        const bool isDBusObjectPathListenersRecordFound =
                        (dbusObjectPathListenersIterator != dbusServiceListenersRecord.dbusObjectPathListenersMap.end());

        if (!isDBusObjectPathListenersRecordFound) {
            continue; // skip
        }

        auto& dbusInterfaceNameListenersMap = dbusObjectPathListenersIterator->second;

        if (dbusInterfaceNamesState == DBusRecordState::RESOLVED) {
            notifyDBusObjectPathResolved(dbusInterfaceNameListenersMap, dbusInterfaceNames);
        } else {
            notifyDBusObjectPathChanged(dbusInterfaceNameListenersMap, dbusInterfaceNames, dbusInterfaceNamesState);
        }

        if (dbusInterfaceNameListenersMap.empty()) {
            dbusServiceListenersRecord.dbusObjectPathListenersMap.erase(dbusObjectPathListenersIterator);
        }
    }

    notificationThread_ = std::thread::id();
}

void DBusServiceRegistry::notifyDBusObjectPathResolved(DBusInterfaceNameListenersMap& dbusInterfaceNameListenersMap,
                                                       const std::unordered_set<std::string>& dbusInterfaceNames) {
    for (auto dbusObjectPathListenersIterator = dbusInterfaceNameListenersMap.begin();
                    dbusObjectPathListenersIterator != dbusInterfaceNameListenersMap.end();) {
        const auto& listenersDBusInterfaceName = dbusObjectPathListenersIterator->first;
        auto& dbusInterfaceNameListenersRecord = dbusObjectPathListenersIterator->second;

        const auto& dbusInterfaceNameIterator = dbusInterfaceNames.find(listenersDBusInterfaceName);
        const bool isDBusInterfaceNameAvailable = (dbusInterfaceNameIterator != dbusInterfaceNames.end());

        notifyDBusInterfaceNameListeners(dbusInterfaceNameListenersRecord, isDBusInterfaceNameAvailable);

        if (dbusInterfaceNameListenersRecord.listenerList.empty()) {
            dbusObjectPathListenersIterator = dbusInterfaceNameListenersMap.erase(dbusObjectPathListenersIterator);
        } else {
            dbusObjectPathListenersIterator++;
        }
    }
}

void DBusServiceRegistry::notifyDBusObjectPathChanged(DBusInterfaceNameListenersMap& dbusInterfaceNameListenersMap,
                                                      const std::unordered_set<std::string>& dbusInterfaceNames,
                                                      const DBusRecordState& dbusInterfaceNamesState) {
    const bool isDBusInterfaceNameAvailable = (dbusInterfaceNamesState == DBusRecordState::AVAILABLE);

    assert(
        dbusInterfaceNamesState == DBusRecordState::AVAILABLE
                        || dbusInterfaceNamesState == DBusRecordState::NOT_AVAILABLE);

    for (const auto& dbusInterfaceName : dbusInterfaceNames) {
        auto dbusInterfaceNameListenersIterator = dbusInterfaceNameListenersMap.find(dbusInterfaceName);
        const bool isDBusInterfaceNameObserved = (dbusInterfaceNameListenersIterator
                        != dbusInterfaceNameListenersMap.end());

        if (isDBusInterfaceNameObserved) {
            auto& dbusInterfaceNameListenersRecord = dbusInterfaceNameListenersIterator->second;

            notifyDBusInterfaceNameListeners(dbusInterfaceNameListenersRecord, isDBusInterfaceNameAvailable);
        }
    }
}

void DBusServiceRegistry::notifyDBusInterfaceNameListeners(DBusInterfaceNameListenersRecord& dbusInterfaceNameListenersRecord,
                                                           const bool& isDBusInterfaceNameAvailable) {
    const AvailabilityStatus availabilityStatus = (isDBusInterfaceNameAvailable ?
                    AvailabilityStatus::AVAILABLE : AvailabilityStatus::NOT_AVAILABLE);
    const DBusRecordState notifyState = (isDBusInterfaceNameAvailable ?
                    DBusRecordState::AVAILABLE : DBusRecordState::NOT_AVAILABLE);

    if (notifyState == dbusInterfaceNameListenersRecord.state) {
        return;
    }
    dbusInterfaceNameListenersRecord.state = notifyState;

    for (auto dbusServiceListenerIterator = dbusInterfaceNameListenersRecord.listenerList.begin();
         dbusServiceListenerIterator != dbusInterfaceNameListenersRecord.listenerList.end();
         dbusServiceListenerIterator++) {
        (*dbusServiceListenerIterator)(availabilityStatus);
    }
}

void DBusServiceRegistry::removeUniqueName(const DBusUniqueNamesMapIterator& dbusUniqueNamesIterator, const std::string &_serviceName) {
    const bool isSubscriptionCancelled = dbusDaemonProxy_->getDBusConnection()->removeObjectManagerSignalMemberHandler(
                    dbusUniqueNamesIterator->first,
                    this);
    assert(isSubscriptionCancelled);
    (void)isSubscriptionCancelled;

    if ("" != _serviceName) {
        auto findServiceName = dbusUniqueNamesIterator->second.ownedBusNames.find(_serviceName);
        if (findServiceName != dbusUniqueNamesIterator->second.ownedBusNames.end())
            dbusUniqueNamesIterator->second.ownedBusNames.erase(findServiceName);
    } else {
        dbusUniqueNamesIterator->second.ownedBusNames.clear();
    }

    if (dbusUniqueNamesIterator->second.ownedBusNames.size() == 0) {
        dbusUniqueNamesMap_.erase(dbusUniqueNamesIterator);
    } else {
        //delete object path cache entry of service
        auto& dbusObjectPathsCache = dbusUniqueNamesIterator->second.dbusObjectPathsCache;
        auto dbusObjectPathCacheIterator = dbusObjectPathsCache.begin();
        while(dbusObjectPathCacheIterator != dbusObjectPathsCache.end()) {
            DBusObjectPathCache *objectPathCache = &(dbusObjectPathCacheIterator->second);
            if(objectPathCache->serviceName == _serviceName) {
                dbusObjectPathCacheIterator = dbusUniqueNamesIterator->second.dbusObjectPathsCache.erase(dbusObjectPathCacheIterator);
            } else {
                ++dbusObjectPathCacheIterator;
            }
        }
    }
}

DBusServiceRegistry::DBusUniqueNameRecord* DBusServiceRegistry::insertServiceNameMapping(const std::string& dbusUniqueName,
                                                                                         const std::string& dbusServiceName) {
    auto* dbusUniqueNameRecord = &(dbusUniqueNamesMap_[dbusUniqueName]);
    dbusUniqueNameRecord->uniqueName = dbusUniqueName;
    dbusUniqueNameRecord->ownedBusNames.insert(dbusServiceName);

    auto dbusServiceNameMapIterator = dbusServiceNameMap_.find(dbusServiceName);

    if(dbusServiceNameMapIterator == dbusServiceNameMap_.end()) {
        dbusServiceNameMap_.insert({ dbusServiceName, dbusUniqueNameRecord });
    }
    else {
        dbusServiceNameMapIterator->second = dbusUniqueNameRecord;
    }

    DBusServiceListenersRecord& dbusServiceListenersRecord = dbusServiceListenersMap[dbusServiceName];

    if(dbusServiceListenersRecord.uniqueBusNameState != DBusRecordState::RESOLVED) {
        dbusServiceListenersRecord.uniqueBusName = dbusUniqueName;
        dbusServiceListenersRecord.uniqueBusNameState = DBusRecordState::UNKNOWN;
    }

    return dbusUniqueNameRecord;
}

/**
 * finds a DBusUniquNameRecord associated with a given well-known name.
 * The returned DBusUniquNameRecord* may be a NULL pointer, if the well-known
 * name is known, but not associated with a unique name yet.
 *
 * @return true, if the given well-known name is found
 */
bool DBusServiceRegistry::findCachedDbusService(
                const std::string& dbusServiceName,
                DBusUniqueNameRecord** uniqueNameRecord) {
    auto dbusUniqueNameRecordIterator = dbusServiceNameMap_.find(dbusServiceName);

    if(dbusUniqueNameRecordIterator != dbusServiceNameMap_.end()) {
        *uniqueNameRecord = dbusUniqueNameRecordIterator->second;
        return true;
    }

    return false;
}

} // namespace DBus
} // namespace CommonAPI
