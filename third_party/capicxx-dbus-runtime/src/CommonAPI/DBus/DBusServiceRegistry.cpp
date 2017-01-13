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

std::mutex DBusServiceRegistry::registriesMutex_;
static CommonAPI::CallInfo serviceRegistryInfo(10000);

std::shared_ptr<DBusServiceRegistry>
DBusServiceRegistry::get(std::shared_ptr<DBusProxyConnection> _connection) {
    std::lock_guard<std::mutex> itsGuard(registriesMutex_);
    auto registries = getRegistryMap();
    auto registryIterator = registries->find(_connection);
    if (registryIterator != registries->end())
        return registryIterator->second;

    std::shared_ptr<DBusServiceRegistry> registry
        = std::make_shared<DBusServiceRegistry>(_connection);
    if (registry) {
        registry->init();
        registries->insert( { _connection, registry } );
    }
    return registry;
}

void
DBusServiceRegistry::remove(std::shared_ptr<DBusProxyConnection> _connection) {
    std::lock_guard<std::mutex> itsGuard(registriesMutex_);
    auto registries = getRegistryMap();
    registries->erase(_connection);
}

DBusServiceRegistry::DBusServiceRegistry(std::shared_ptr<DBusProxyConnection> dbusProxyConnection) :
                dbusDaemonProxy_(std::make_shared<CommonAPI::DBus::DBusDaemonProxy>(dbusProxyConnection)),
                initialized_(false),
                notificationThread_(),
                registries_(getRegistryMap()) {
}

DBusServiceRegistry::~DBusServiceRegistry() {
    if (!initialized_) {
        return;
    }

    dbusDaemonProxy_->getNameOwnerChangedEvent().unsubscribe(dbusDaemonProxyNameOwnerChangedEventSubscription_);

    // notify only listeners of resolved services (online > offline)
    for (auto& dbusServiceListenersIterator : dbusServiceListenersMap) {
        auto& dbusServiceListenersRecord = dbusServiceListenersIterator.second;

        if (dbusServiceListenersRecord.uniqueBusNameState == DBusRecordState::RESOLVED) {
            dbusServicesMutex_.lock();
            onDBusServiceNotAvailable(dbusServiceListenersRecord);
            dbusServicesMutex_.unlock();
        }
    }

    // remove all object manager signal member handlers
    for (auto& dbusServiceUniqueNameIterator : dbusUniqueNamesMap_) {
        const auto& dbusServiceUniqueName = dbusServiceUniqueNameIterator.first;

        auto dbusProxyConnection = dbusDaemonProxy_->getDBusConnection();
        const bool isSubscriptionCancelled = dbusProxyConnection->removeObjectManagerSignalMemberHandler(
            dbusServiceUniqueName,
            this);
        if (!isSubscriptionCancelled) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), ": still subscribed too ", std::string(dbusServiceUniqueName));
        }
    }
}

void DBusServiceRegistry::init() {
    selfReference_ = shared_from_this();
    translator_ = DBusAddressTranslator::get();

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
        const std::string &_address,
        DBusServiceListener serviceListener,
        std::weak_ptr<DBusProxy> _proxy) {
    DBusAddress dbusAddress;
    translator_->translate(_address, dbusAddress);

    if (notificationThread_ == std::this_thread::get_id()) {
        COMMONAPI_ERROR(
            "You must not build proxies in callbacks of ProxyStatusEvent.",
            " Please refer to the documentation for suggestions how to avoid this.");
    }

    dbusServicesMutex_.lock();
    auto& dbusServiceListenersRecord = dbusServiceListenersMap[dbusAddress.getService()];
    if (dbusServiceListenersRecord.uniqueBusNameState == DBusRecordState::AVAILABLE) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " uniqueBusName ", dbusServiceListenersRecord.uniqueBusName, " already AVAILABLE");
    }

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
        if(auto itsProxy = _proxy.lock())
            serviceListener(itsProxy, availabilityStatus);
        notificationThread_ = std::thread::id();
    }


    DBusServiceSubscription subscriptionKey = dbusInterfaceNameListenersRecord.nextSubscriptionKey++;
    std::shared_ptr<DBusServiceListenerInfo> info = std::make_shared<DBusServiceListenerInfo>();
    info->listener = std::move(serviceListener);
    info->proxy = _proxy;
    dbusInterfaceNameListenersRecord.listenerList.insert(std::make_pair(subscriptionKey, info));
    dbusInterfaceNameListenersRecord.listenersToRemove.remove(subscriptionKey);
    dbusServicesMutex_.unlock();

    return subscriptionKey;
}

void
DBusServiceRegistry::unsubscribeAvailabilityListener(
    const std::string &_address, DBusServiceSubscription& listenerSubscription) {
    DBusAddress dbusAddress;
    translator_->translate(_address, dbusAddress);

    dbusServicesMutex_.lock();
    auto dbusServiceListenersIterator = dbusServiceListenersMap.find(dbusAddress.getService());
    const bool isDBusServiceListenersRecordFound = (dbusServiceListenersIterator != dbusServiceListenersMap.end());

    if (!isDBusServiceListenersRecordFound) {
        dbusServicesMutex_.unlock();
        return; // already unsubscribed
    }

    auto& dbusServiceListenersRecord = dbusServiceListenersIterator->second;
    auto dbusObjectPathListenersIterator =
                    dbusServiceListenersRecord.dbusObjectPathListenersMap.find(dbusAddress.getObjectPath());
    const bool isDBusObjectPathListenersRecordFound =
                    (dbusObjectPathListenersIterator != dbusServiceListenersRecord.dbusObjectPathListenersMap.end());

    if (!isDBusObjectPathListenersRecordFound) {
        dbusServicesMutex_.unlock();
        return; // already unsubscribed
    }

    auto& dbusInterfaceNameListenersMap = dbusObjectPathListenersIterator->second;
    auto dbusInterfaceNameListenersIterator = dbusInterfaceNameListenersMap.find(dbusAddress.getInterface());
    const bool isDBusInterfaceNameListenersRecordFound =
                    (dbusInterfaceNameListenersIterator != dbusInterfaceNameListenersMap.end());

    if (!isDBusInterfaceNameListenersRecordFound) {
        dbusServicesMutex_.unlock();
        return; // already unsubscribed
    }

    auto& dbusInterfaceNameListenersRecord = dbusInterfaceNameListenersIterator->second;

    // mark listener to remove
    dbusInterfaceNameListenersRecord.listenersToRemove.push_back(listenerSubscription);

    dbusServicesMutex_.unlock();
}

// d-feet mode until service is found
bool DBusServiceRegistry::isServiceInstanceAlive(const std::string& dbusInterfaceName,
                                                 const std::string& dbusServiceName,
                                                 const std::string& dbusObjectPath) {
    bool result = false;
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

        if(futureNameResolved.get() != DBusRecordState::RESOLVED)
            return false;

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

    if (NULL == dbusUniqueNameRecord) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " no unique name record found for IF: ", dbusInterfaceName,
                        " service: ", dbusServiceName, "object path: ", dbusObjectPath);
    }

    if(dbusPredefinedServices_.find(dbusServiceName) == dbusPredefinedServices_.end()) {

        auto& dbusObjectPathsCache = dbusUniqueNameRecord->dbusObjectPathsCache;
        auto dbusObjectPathCacheIterator = dbusObjectPathsCache.find(dbusObjectPath);

        DBusObjectPathCache* dbusObjectPathCache = NULL;

        if(dbusObjectPathCacheIterator != dbusObjectPathsCache.end()) {
            dbusObjectPathCache = &(dbusObjectPathCacheIterator->second);
            if (dbusObjectPathCache->state != DBusRecordState::RESOLVED) {
                dbusObjectPathCache->state = DBusRecordState::RESOLVING;

                dbusObjectPathCache = &(dbusObjectPathCacheIterator->second);

                std::future<DBusRecordState> futureObjectPathResolved = dbusObjectPathCache->promiseOnResolve.get_future();
                dbusServicesMutex_.unlock();

                resolveObjectPathWithObjectManager(uniqueName, dbusObjectPath);
                futureObjectPathResolved.wait_for(timeout);
            } else {
                dbusServicesMutex_.unlock();
            }
        }
        else {
            // try to resolve object paths
            DBusObjectPathCache newDbusObjectPathCache;
            newDbusObjectPathCache.state = DBusRecordState::RESOLVING;
            newDbusObjectPathCache.serviceName = dbusServiceName;

            dbusObjectPathsCache.insert(std::make_pair(dbusObjectPath, std::move(newDbusObjectPathCache)));

            dbusObjectPathCacheIterator = dbusObjectPathsCache.find(dbusObjectPath);

            dbusObjectPathCache = &(dbusObjectPathCacheIterator->second);

            newDbusObjectPathCache.futureOnResolve = dbusObjectPathCache->promiseOnResolve.get_future();
            dbusServicesMutex_.unlock();

            resolveObjectPathWithObjectManager(uniqueName, dbusObjectPath);
            newDbusObjectPathCache.futureOnResolve.wait_for(timeout);
        }

        if (NULL == dbusObjectPathCache) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " no object path cache entry found for IF: ", dbusInterfaceName,
                            " service: ", dbusServiceName, "object path: ", dbusObjectPath);
        }

        dbusServicesMutex_.lock();
        if(dbusObjectPathCache->state != DBusRecordState::RESOLVED) {
            dbusServicesMutex_.unlock();
            return false;
        }

        auto dbusInterfaceNamesIterator = dbusObjectPathCache->dbusInterfaceNamesCache.find(dbusInterfaceName);
        result = dbusInterfaceNamesIterator != dbusObjectPathCache->dbusInterfaceNamesCache.end();
        dbusServicesMutex_.unlock();

    } else {
        //service is predefined
        result = true;
        dbusServicesMutex_.unlock();
    }

    return(result);
}

void DBusServiceRegistry::fetchAllServiceNames() {
    if (!dbusDaemonProxy_->isAvailable()) {
        return;
    }

    CallStatus callStatus;
    std::vector<std::string> availableServiceNames;

    dbusDaemonProxy_->listNames(callStatus, availableServiceNames);

    dbusServicesMutex_.lock();
    if (callStatus == CallStatus::SUCCESS) {
        for(std::string serviceName : availableServiceNames) {
            if(isDBusServiceName(serviceName)) {
                dbusServiceNameMap_[serviceName];
            }
        }
    }
    dbusServicesMutex_.unlock();
}

void DBusServiceRegistry::getAvailableServiceInstances(const std::string& dbusServiceName,
                                                       const std::string& dbusObjectPath,
                                                       DBusObjectManagerStub::DBusObjectPathAndInterfacesDict& availableServiceInstances) {
    getManagedObjects(dbusServiceName, dbusObjectPath, availableServiceInstances);
}

void DBusServiceRegistry::getAvailableServiceInstancesAsync(GetAvailableServiceInstancesCallback callback,
                                                   const std::string& dbusServiceName,
                                                   const std::string& dbusObjectPath) {
    getManagedObjectsAsync(dbusServiceName, dbusObjectPath, [callback](const CallStatus& callStatus,
            const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict availableServiceInstances,
            const std::string& dbusServiceName,
            const std::string& dbusObjectPath) {
        (void)callStatus;
        (void)dbusServiceName;
        (void)dbusObjectPath;
        callback(availableServiceInstances);
    });
}

void DBusServiceRegistry::onSignalDBusMessage(const DBusMessage &_dbusMessage) {
    const std::string& dbusServiceUniqueName = _dbusMessage.getSender();

    if (!_dbusMessage.isSignalType()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " signal message expected, got ", _dbusMessage.getMember(), " type: ", int(_dbusMessage.getType()));
    }
    if (!_dbusMessage.hasInterfaceName("org.freedesktop.DBus.ObjectManager")) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " unexpected interface ", _dbusMessage.getInterface());
    }
    if (!_dbusMessage.hasMemberName("InterfacesAdded") && !_dbusMessage.hasMemberName("InterfacesAdded") &&
            !_dbusMessage.hasMemberName("InterfacesRemoved") && !_dbusMessage.hasMemberName("InterfacesRemoved") ) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " unexpected member ", _dbusMessage.getMember());
    }

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
            if (dbusInputStream.hasError()) {
                COMMONAPI_ERROR(std::string(__FUNCTION__), " input stream error");
            }
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

    dbusServicesMutex_.lock();

    auto dbusServiceUniqueNameIterator = dbusUniqueNamesMap_.find(dbusServiceUniqueName);
    const bool isDBusServiceUniqueNameFound = (dbusServiceUniqueNameIterator != dbusUniqueNamesMap_.end());

    if (!isDBusServiceUniqueNameFound) {
        // LB TODO: unsubscribe here!
        // Needs to be reworked in order to store the subscription identifier!
        dbusServicesMutex_.unlock();
        return;
    }

    auto& dbusUniqueNameRecord = dbusServiceUniqueNameIterator->second;

    DBusObjectPathCache *dbusObjectPathRecord;
    auto dbusObjectPathCacheIterator = dbusUniqueNameRecord.dbusObjectPathsCache.find(dbusObjectPath);
    if(dbusObjectPathCacheIterator != dbusUniqueNameRecord.dbusObjectPathsCache.end()) {
        dbusObjectPathRecord = &(dbusObjectPathCacheIterator->second);
    } else {
        dbusServicesMutex_.unlock();
        return;
    }

    if (dbusObjectPathRecord->state != DBusRecordState::RESOLVED) {
        dbusServicesMutex_.unlock();
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

    dbusServicesMutex_.unlock();
}

void DBusServiceRegistry::setDBusServicePredefined(const std::string& _serviceName) {
    dbusPredefinedServices_.insert(_serviceName);
}

void DBusServiceRegistry::resolveDBusServiceName(const std::string& dbusServiceName,
                                                 DBusServiceListenersRecord& dbusServiceListenersRecord) {
    if (dbusServiceListenersRecord.uniqueBusNameState == DBusRecordState::RESOLVED) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " already resolved ", dbusServiceName);
    }
    if (!dbusServiceListenersRecord.uniqueBusName.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " unique name not empty ", dbusServiceListenersRecord.uniqueBusName);
    }

    if (dbusDaemonProxy_->isAvailable()) {

        auto func = std::bind(
            &DBusServiceRegistry::onGetNameOwnerCallback,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2,
            dbusServiceName);

        DBusProxyAsyncCallbackHandler<DBusServiceRegistry,
                                      std::string>::Delegate delegate(shared_from_this(), func);

        dbusDaemonProxy_->getNameOwnerAsync<DBusServiceRegistry>(dbusServiceName, delegate);

        dbusServiceListenersRecord.uniqueBusNameState = DBusRecordState::RESOLVING;
    }
}

void DBusServiceRegistry::onGetNameOwnerCallback(const CallStatus& status,
                                                 std::string dbusServiceUniqueName,
                                                 const std::string& dbusServiceName) {
    dbusServicesMutex_.lock();

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

    dbusServicesMutex_.unlock();
}

DBusServiceRegistry::DBusRecordState
DBusServiceRegistry::resolveDBusInterfaceNameState(
    const DBusAddress &_dbusAddress, DBusServiceListenersRecord &dbusServiceListenersRecord) {

    if (dbusServiceListenersRecord.uniqueBusNameState != DBusRecordState::RESOLVED) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " unresolved ", dbusServiceListenersRecord.uniqueBusName);
    }
    if (dbusServiceListenersRecord.uniqueBusName.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " unique bus name is empty");
    }

    auto& dbusServiceUniqueNameRecord = dbusUniqueNamesMap_[dbusServiceListenersRecord.uniqueBusName];
    if (dbusServiceListenersRecord.uniqueBusName.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " empty ownedBusNames");
    }

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
            selfReference_);
        if (!isSubscriptionSuccessful) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " cannot subscribe too ", dbusServiceUniqueName);
        }
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

    if (dbusServiceListenersRecord.uniqueBusName.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " unique bus name is empty");
    }

    auto& dbusUniqueNameRecord = dbusUniqueNamesMap_[dbusServiceListenersRecord.uniqueBusName];
    if (dbusServiceListenersRecord.uniqueBusName.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " empty ownedBusNames");
    }
    if (dbusUniqueNameRecord.dbusObjectPathsCache.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " empty dbusObjectPathsCache");
    }

    auto dbusObjectPathCacheIterator = dbusUniqueNameRecord.dbusObjectPathsCache.find(dbusObjectPath);
    const bool isDBusObjectPathCacheFound = (dbusObjectPathCacheIterator != dbusUniqueNameRecord.dbusObjectPathsCache.end());
    if (!isDBusObjectPathCacheFound) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " no object path cache entry found for ", dbusObjectPath);
    }

    auto& dbusObjectPathCache = dbusObjectPathCacheIterator->second;
    if (0 == dbusObjectPathCache.referenceCount) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " reference count is 0");
    }

    dbusObjectPathCache.referenceCount--;

    if (dbusObjectPathCache.referenceCount == 0) {
        dbusUniqueNameRecord.dbusObjectPathsCache.erase(dbusObjectPathCacheIterator);

        const bool isLastDBusObjectPathCache = dbusUniqueNameRecord.dbusObjectPathsCache.empty();
        if (isLastDBusObjectPathCache) {
            dbusServicesMutex_.unlock();
            auto dbusProxyConnection = dbusDaemonProxy_->getDBusConnection();
            const bool isSubscriptionCancelled = dbusProxyConnection->removeObjectManagerSignalMemberHandler(
                dbusServiceListenersRecord.uniqueBusName,
                this);
            if (!isSubscriptionCancelled) {
                COMMONAPI_ERROR(std::string(__FUNCTION__), ": still subscribed too ", dbusServiceListenersRecord.uniqueBusName);
            }
            dbusServicesMutex_.lock();
        }
    }
}

bool DBusServiceRegistry::resolveObjectPathWithObjectManager(const std::string& dbusServiceUniqueName, const std::string& dbusObjectPath) {
    // get managed objects from root object manager
    auto getManagedObjectsCallback = std::bind(
            &DBusServiceRegistry::onGetManagedObjectsCallbackResolve,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2,
            dbusServiceUniqueName,
            dbusObjectPath);
    return getManagedObjectsAsync(dbusServiceUniqueName, "/", getManagedObjectsCallback);
}

bool DBusServiceRegistry::getManagedObjects(const std::string& dbusServiceName,
                            const std::string& dbusObjectPath,
                            DBusObjectManagerStub::DBusObjectPathAndInterfacesDict& availableServiceInstances) {
    auto dbusConnection = dbusDaemonProxy_->getDBusConnection();

    if (dbusServiceName.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " dbusServiceName empty");
    }

    if(dbusConnection->isConnected()) {

        DBusAddress dbusAddress(dbusServiceName, dbusObjectPath, "org.freedesktop.DBus.ObjectManager");
        DBusMessage dbusMessageCall = CommonAPI::DBus::DBusMessage::createMethodCall(
                dbusAddress,
                "GetManagedObjects");

        DBusError error;
        CallInfo* defaultCallInfo = new CallInfo();
        DBusMessage reply = dbusConnection->sendDBusMessageWithReplyAndBlock(dbusMessageCall, error, defaultCallInfo);
        delete defaultCallInfo;

        DBusInputStream input(reply);
        if (!DBusSerializableArguments<DBusObjectManagerStub::DBusObjectPathAndInterfacesDict>::deserialize(
                input, availableServiceInstances) || error)
            return false;
    }
    return true;
}

bool DBusServiceRegistry::getManagedObjectsAsync(const std::string& dbusServiceName,
                                                 const std::string& dbusObjectPath,
                                                 GetManagedObjectsCallback callback) {
    bool isSendingInProgress = false;
    auto dbusConnection = dbusDaemonProxy_->getDBusConnection();

    if (dbusServiceName.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " dbusServiceName empty");
    }

    if(dbusConnection->isConnected()) {

        DBusAddress dbusAddress(dbusServiceName, dbusObjectPath, "org.freedesktop.DBus.ObjectManager");
        DBusMessage dbusMessageCall = CommonAPI::DBus::DBusMessage::createMethodCall(
                dbusAddress,
                "GetManagedObjects");

        auto getManagedObjectsCallback = std::bind(
                callback,
                std::placeholders::_1,
                std::placeholders::_2,
                dbusServiceName,
                dbusObjectPath);

        DBusProxyAsyncCallbackHandler<
                            DBusServiceRegistry,
                            DBusObjectManagerStub::DBusObjectPathAndInterfacesDict
                        >::Delegate delegate(shared_from_this(), getManagedObjectsCallback);

        dbusConnection->sendDBusMessageWithReplyAsync(
                dbusMessageCall,
                DBusProxyAsyncCallbackHandler<
                    DBusServiceRegistry,
                    DBusObjectManagerStub::DBusObjectPathAndInterfacesDict
                >::create(delegate, std::tuple<DBusObjectManagerStub::DBusObjectPathAndInterfacesDict>()),
                &serviceRegistryInfo);

        isSendingInProgress = true;
    }
    return isSendingInProgress;
}

void DBusServiceRegistry::onGetManagedObjectsCallbackResolve(const CallStatus& callStatus,
                                 const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict availableServiceInstances,
                                 const std::string& dbusServiceUniqueName,
                                 const std::string& dbusObjectPath) {

    if(callStatus == CallStatus::SUCCESS) {
        //has object manager
        bool objectPathFound = false;
        for(auto objectPathDict : availableServiceInstances)
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
        }

        if(!objectPathFound) {
            // check if the main part of the object path is in the list.
            // if it is, the object path could be managed.
            // else, it maybe existed a while back but is now gone, in which case just ignore.

            std::string objectPathManager = dbusObjectPath.substr(0, dbusObjectPath.find_last_of("\\/"));
            for(auto objectPathDict : availableServiceInstances)
            {

                std::string objectPath = objectPathDict.first;

                if (dbusObjectPath.substr(0, objectPath.size()) != objectPath)
                    continue;

                // also check that the next character in dbusObject path is a slash or a backslash,
                // so that we can make sure that we have compared against a full path element
                auto delimiter = dbusObjectPath.at(objectPath.size());
                if (delimiter != '\\' && delimiter != '/')
                    continue;

                // object path is managed. Try to resolve object path with the help of the manager
                auto getManagedObjectsCallback = std::bind(
                        &DBusServiceRegistry::onGetManagedObjectsCallbackResolve,
                        shared_from_this(),
                        std::placeholders::_1,
                        std::placeholders::_2,
                        dbusServiceUniqueName,
                        dbusObjectPath);
                getManagedObjectsAsync(dbusServiceUniqueName, objectPathManager, getManagedObjectsCallback);

            }

        }
    } else {
        COMMONAPI_ERROR("There is no Object Manager that manages " + dbusObjectPath + ". Resolving failed!");
    }
}

void DBusServiceRegistry::processManagedObject(const std::string& dbusObjectPath,
                                               const std::string& dbusServiceUniqueName,
                                               const std::string& interfaceName) {

    auto dbusServiceUniqueNameIterator = dbusUniqueNamesMap_.find(dbusServiceUniqueName);
    const bool isDBusServiceUniqueNameFound = (dbusServiceUniqueNameIterator != dbusUniqueNamesMap_.end());

    if (!isDBusServiceUniqueNameFound)
        return;

    DBusUniqueNameRecord& dbusUniqueNameRecord = dbusServiceUniqueNameIterator->second;
    auto dbusObjectPathIterator = dbusUniqueNameRecord.dbusObjectPathsCache.find(dbusObjectPath);
    const bool isDBusObjectPathFound = (dbusObjectPathIterator != dbusUniqueNameRecord.dbusObjectPathsCache.end());

    if (!isDBusObjectPathFound)
        return;

    DBusObjectPathCache& dbusObjectPathRecord = dbusObjectPathIterator->second;

    if(!isOrgFreedesktopDBusInterface(interfaceName)) {
        dbusObjectPathRecord.dbusInterfaceNamesCache.insert(interfaceName);
    } else if (translator_->isOrgFreedesktopDBusPeerMapped() && (interfaceName == "org.freedesktop.DBus.Peer")) {
        dbusObjectPathRecord.dbusInterfaceNamesCache.insert(interfaceName);
    }

    dbusObjectPathRecord.state = DBusRecordState::RESOLVED;
    if(dbusObjectPathRecord.futureOnResolve.valid())
        dbusObjectPathRecord.promiseOnResolve.set_value(dbusObjectPathRecord.state);

    dbusUniqueNameRecord.objectPathsState = DBusRecordState::RESOLVED;

    notifyDBusServiceListeners(
        dbusUniqueNameRecord,
        dbusObjectPath,
        dbusObjectPathRecord.dbusInterfaceNamesCache,
        DBusRecordState::RESOLVED);
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

    dbusServicesMutex_.lock();

    if (isDBusServiceNameLost) {
        checkDBusServiceWasAvailable(affectedName, dbusServiceUniqueName);
    } else {
        onDBusServiceAvailable(affectedName, dbusServiceUniqueName);
    }

    dbusServicesMutex_.unlock();

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
    const std::unordered_set<std::string> dbusInterfaceNamesCache {};

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

    if ((dbusInterfaceNamesState != DBusRecordState::AVAILABLE) && (dbusInterfaceNamesState != DBusRecordState::NOT_AVAILABLE)) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " unexpected state ", int(dbusInterfaceNamesState));
    }

    for (const auto& dbusInterfaceName : dbusInterfaceNames) {
        auto dbusInterfaceNameListenersIterator = dbusInterfaceNameListenersMap.find(dbusInterfaceName);
        const bool isDBusInterfaceNameObserved = (dbusInterfaceNameListenersIterator
                        != dbusInterfaceNameListenersMap.end());

        if (isDBusInterfaceNameObserved) {
            auto& dbusInterfaceNameListenersRecord = dbusInterfaceNameListenersIterator->second;
            notifyDBusInterfaceNameListeners(dbusInterfaceNameListenersRecord, isDBusInterfaceNameAvailable);

            if (dbusInterfaceNameListenersRecord.listenerList.empty())
                dbusInterfaceNameListenersMap.erase(dbusInterfaceNameListenersIterator);
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

    auto dbusServiceListenerIterator = dbusInterfaceNameListenersRecord.listenerList.begin();
    while (dbusServiceListenerIterator != dbusInterfaceNameListenersRecord.listenerList.end()) {

        auto itsRemoveListenerIt = std::find(dbusInterfaceNameListenersRecord.listenersToRemove.begin(),
                  dbusInterfaceNameListenersRecord.listenersToRemove.end(),
                  dbusServiceListenerIterator->first);

        if(itsRemoveListenerIt != dbusInterfaceNameListenersRecord.listenersToRemove.end()) {
            dbusInterfaceNameListenersRecord.listenersToRemove.remove(dbusServiceListenerIterator->first);
            dbusServiceListenerIterator = dbusInterfaceNameListenersRecord.listenerList.erase(dbusServiceListenerIterator);
        } else {
            if(auto itsProxy = dbusServiceListenerIterator->second->proxy.lock())
                (dbusServiceListenerIterator->second->listener)(itsProxy, availabilityStatus);
            ++dbusServiceListenerIterator;
        }
    }
}

void DBusServiceRegistry::removeUniqueName(const DBusUniqueNamesMapIterator& dbusUniqueNamesIterator, const std::string &_serviceName) {
    if ("" != _serviceName) {
        auto findServiceName = dbusUniqueNamesIterator->second.ownedBusNames.find(_serviceName);
        if (findServiceName != dbusUniqueNamesIterator->second.ownedBusNames.end())
            dbusUniqueNamesIterator->second.ownedBusNames.erase(findServiceName);
    } else {
        dbusUniqueNamesIterator->second.ownedBusNames.clear();
    }

    if (dbusUniqueNamesIterator->second.ownedBusNames.size() == 0) {
        std::string dbusUniqueName = dbusUniqueNamesIterator->first;
        dbusUniqueNamesMap_.erase(dbusUniqueNamesIterator);
        dbusServicesMutex_.unlock();
        const bool isSubscriptionCancelled = dbusDaemonProxy_->getDBusConnection()->removeObjectManagerSignalMemberHandler(
                dbusUniqueName,
                this);
        if (!isSubscriptionCancelled) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), ": still subscribed too ", dbusUniqueName);
        }
        dbusServicesMutex_.lock();
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
