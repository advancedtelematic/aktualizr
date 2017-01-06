// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSSERVICEREGISTRY_HPP_
#define COMMONAPI_DBUS_DBUSSERVICEREGISTRY_HPP_

#include <algorithm>
#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <list>

#include <pugixml/pugixml.hpp>

#include <CommonAPI/Attribute.hpp>
#include <CommonAPI/Proxy.hpp>
#include <CommonAPI/Types.hpp>
#include <CommonAPI/DBus/DBusProxyConnection.hpp>
#include <CommonAPI/DBus/DBusFactory.hpp>
#include <CommonAPI/DBus/DBusObjectManagerStub.hpp>

namespace CommonAPI {
namespace DBus {

typedef Event<std::string, std::string, std::string> NameOwnerChangedEvent;
typedef Event<std::string, std::string, std::string>::Subscription NameOwnerChangedEventSubscription;

// Connection name, Object path
typedef std::pair<std::string, std::string> DBusInstanceId;

class DBusAddress;
class DBusAddressTranslator;
class DBusDaemonProxy;

class DBusServiceRegistry: public std::enable_shared_from_this<DBusServiceRegistry>,
                           public DBusProxyConnection::DBusSignalHandler {
 public:
    enum class DBusRecordState {
        UNKNOWN,
        AVAILABLE,
        RESOLVING,
        RESOLVED,
        NOT_AVAILABLE
    };

    // template class DBusServiceListener<> { typedef functor; typedef list; typedef subscription }
    typedef std::function<void(std::shared_ptr<DBusProxy>, const AvailabilityStatus& availabilityStatus)> DBusServiceListener;
    typedef long int DBusServiceSubscription;
    struct DBusServiceListenerInfo {
        DBusServiceListener listener;
        std::weak_ptr<DBusProxy> proxy;
    };
    typedef std::map<DBusServiceSubscription, std::shared_ptr<DBusServiceListenerInfo>> DBusServiceListenerList;

    typedef std::function<void(const std::vector<std::string>& interfaces,
                               const AvailabilityStatus& availabilityStatus)> DBusManagedInterfaceListener;
    typedef std::list<DBusManagedInterfaceListener> DBusManagedInterfaceListenerList;
    typedef DBusManagedInterfaceListenerList::iterator DBusManagedInterfaceSubscription;

    typedef std::function<void(const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict)> GetAvailableServiceInstancesCallback;

    static std::shared_ptr<DBusServiceRegistry> get(std::shared_ptr<DBusProxyConnection> _connection);
    static void remove(std::shared_ptr<DBusProxyConnection> _connection);

    DBusServiceRegistry(std::shared_ptr<DBusProxyConnection> dbusProxyConnection);

    DBusServiceRegistry(const DBusServiceRegistry&) = delete;
    DBusServiceRegistry& operator=(const DBusServiceRegistry&) = delete;

    virtual ~DBusServiceRegistry();

    void init();

    DBusServiceSubscription subscribeAvailabilityListener(const std::string &_address,
                                                          DBusServiceListener _listener,
                                                          std::weak_ptr<DBusProxy> _proxy);

    void unsubscribeAvailabilityListener(const std::string &_address,
                                         DBusServiceSubscription &_listener);


    bool isServiceInstanceAlive(const std::string &_dbusInterfaceName,
                                const std::string &_dbusConnectionName,
                                const std::string &_dbusObjectPath);


    virtual void getAvailableServiceInstances(const std::string& dbusServiceName,
            const std::string& dbusObjectPath,
            DBusObjectManagerStub::DBusObjectPathAndInterfacesDict& availableServiceInstances);

    virtual void getAvailableServiceInstancesAsync(GetAvailableServiceInstancesCallback callback,
                                                   const std::string& dbusServiceName,
                                                   const std::string& dbusObjectPath);

    virtual void onSignalDBusMessage(const DBusMessage&);

    void setDBusServicePredefined(const std::string& _serviceName);

 private:
    struct DBusInterfaceNameListenersRecord {
        DBusInterfaceNameListenersRecord()
            : state(DBusRecordState::UNKNOWN),
              nextSubscriptionKey(0) {
        }

        DBusInterfaceNameListenersRecord(DBusInterfaceNameListenersRecord &&_other)
            : state(_other.state),
              listenerList(std::move(_other.listenerList)),
              listenersToRemove(std::move(_other.listenersToRemove)),
              nextSubscriptionKey(_other.nextSubscriptionKey){
        }

        DBusRecordState state;
        DBusServiceListenerList listenerList;
        std::list<DBusServiceSubscription> listenersToRemove;
        DBusServiceSubscription nextSubscriptionKey;
    };

    typedef std::unordered_map<std::string, DBusInterfaceNameListenersRecord> DBusInterfaceNameListenersMap;

    struct DBusServiceListenersRecord {
        DBusServiceListenersRecord()
            : uniqueBusNameState(DBusRecordState::UNKNOWN),
              mutexOnResolve() {
        }

        DBusServiceListenersRecord(DBusServiceListenersRecord&& other)
            : uniqueBusNameState(other.uniqueBusNameState),
              uniqueBusName(std::move(other.uniqueBusName)),
              promiseOnResolve(std::move(other.promiseOnResolve)),
              futureOnResolve(std::move(other.futureOnResolve)),
              mutexOnResolve(std::move(other.mutexOnResolve)),
              dbusObjectPathListenersMap(std::move(other.dbusObjectPathListenersMap)) {
        }

        ~DBusServiceListenersRecord() {};

        DBusRecordState uniqueBusNameState;
        std::string uniqueBusName;

        std::promise<DBusRecordState> promiseOnResolve;
        std::shared_future<DBusRecordState> futureOnResolve;
        std::unique_lock<std::mutex>* mutexOnResolve;

        std::unordered_map<std::string, DBusInterfaceNameListenersMap> dbusObjectPathListenersMap;
    };

    std::unordered_map<std::string, DBusServiceListenersRecord> dbusServiceListenersMap;


    struct DBusObjectPathCache {
        DBusObjectPathCache()
            : referenceCount(0),
              state(DBusRecordState::UNKNOWN){
        }

        DBusObjectPathCache(DBusObjectPathCache&& other)
            : referenceCount(other.referenceCount),
              state(other.state),
              promiseOnResolve(std::move(other.promiseOnResolve)),
              futureOnResolve(std::move(other.futureOnResolve)),
              serviceName(std::move(other.serviceName)),
              dbusInterfaceNamesCache(std::move(other.dbusInterfaceNamesCache)){
        }

        ~DBusObjectPathCache() {}

        size_t referenceCount;
        DBusRecordState state;
        std::promise<DBusRecordState> promiseOnResolve;
        std::shared_future<DBusRecordState> futureOnResolve;
        std::string serviceName;

        std::unordered_set<std::string> dbusInterfaceNamesCache;
    };

    struct DBusUniqueNameRecord {
        DBusUniqueNameRecord()
            : objectPathsState(DBusRecordState::UNKNOWN) {
        }

        DBusUniqueNameRecord(DBusUniqueNameRecord&& other)
            : uniqueName(std::move(other.uniqueName)),
              objectPathsState(other.objectPathsState),
              ownedBusNames(std::move(other.ownedBusNames)),
              dbusObjectPathsCache(std::move(other.dbusObjectPathsCache)) {
        }

        std::string uniqueName;
        DBusRecordState objectPathsState;
        std::unordered_set<std::string> ownedBusNames;
        std::unordered_map<std::string, DBusObjectPathCache> dbusObjectPathsCache;
    };

    std::unordered_map<std::string, DBusUniqueNameRecord> dbusUniqueNamesMap_;
    typedef std::unordered_map<std::string, DBusUniqueNameRecord>::iterator DBusUniqueNamesMapIterator;

    // mapping service names (well-known names) to service instances
    std::unordered_map<std::string, DBusUniqueNameRecord*> dbusServiceNameMap_;

    // protects the dbus service maps
    std::recursive_mutex dbusServicesMutex_;

    void resolveDBusServiceName(const std::string& dbusServiceName,
                                DBusServiceListenersRecord& dbusServiceListenersRecord);

    void onGetNameOwnerCallback(const CallStatus& status,
                                std::string dbusServiceUniqueName,
                                const std::string& dbusServiceName);


    DBusRecordState resolveDBusInterfaceNameState(const DBusAddress &_address,
                                                  DBusServiceListenersRecord &_record);


    DBusObjectPathCache& getDBusObjectPathCacheReference(const std::string& dbusObjectPath,
                                                         const std::string& dbusServiceName,
                                                         const std::string& dbusServiceUniqueName,
                                                         DBusUniqueNameRecord& dbusUniqueNameRecord);

    void releaseDBusObjectPathCacheReference(const std::string& dbusObjectPath,
                                             const DBusServiceListenersRecord& dbusServiceListenersRecord);

    bool resolveObjectPathWithObjectManager(const std::string& dbusServiceUniqueName, const std::string& dbusObjectPath);

    typedef std::function<void(const CallStatus&,
                               const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict,
                               const std::string&,
                               const std::string&)> GetManagedObjectsCallback;

    bool getManagedObjects(const std::string& dbusServiceName,
                           const std::string& dbusObjectPath,
                           DBusObjectManagerStub::DBusObjectPathAndInterfacesDict& availableServiceInstances);

    bool getManagedObjectsAsync(const std::string& dbusServiceName,
                                const std::string& dbusObjectPath,
                                GetManagedObjectsCallback callback);

    void onGetManagedObjectsCallbackResolve(const CallStatus& callStatus,
                                     const DBusObjectManagerStub::DBusObjectPathAndInterfacesDict availableServiceInstances,
                                     const std::string& dbusServiceUniqueName,
                                     const std::string& dbusObjectPath);

    void processManagedObject(const std::string& dbusObjectPath,
                              const std::string& dbusServiceUniqueName,
                              const std::string& interfaceName);

    void onDBusDaemonProxyNameOwnerChangedEvent(const std::string& name,
                                                const std::string& oldOwner,
                                                const std::string& newOwner);

    std::shared_ptr<DBusDaemonProxy> dbusDaemonProxy_;
    bool initialized_;

    ProxyStatusEvent::Subscription dbusDaemonProxyStatusEventSubscription_;
    NameOwnerChangedEvent::Subscription dbusDaemonProxyNameOwnerChangedEventSubscription_;


    void checkDBusServiceWasAvailable(const std::string& dbusServiceName, const std::string& dbusServiceUniqueName);

    void onDBusServiceAvailable(const std::string& dbusServiceName, const std::string& dbusServiceUniqueName);

    void onDBusServiceNotAvailable(DBusServiceListenersRecord& dbusServiceListenersRecord, const std::string &_serviceName = "");


    void notifyDBusServiceListeners(const DBusUniqueNameRecord& dbusUniqueNameRecord,
                                    const std::string& dbusObjectPath,
                                    const std::unordered_set<std::string>& dbusInterfaceNames,
                                    const DBusRecordState& dbusInterfaceNamesState);

    void notifyDBusObjectPathResolved(DBusInterfaceNameListenersMap& dbusInterfaceNameListenersMap,
                                      const std::unordered_set<std::string>& dbusInterfaceNames);

    void notifyDBusObjectPathChanged(DBusInterfaceNameListenersMap& dbusInterfaceNameListenersMap,
                                     const std::unordered_set<std::string>& dbusInterfaceNames,
                                     const DBusRecordState& dbusInterfaceNamesState);

    void notifyDBusInterfaceNameListeners(DBusInterfaceNameListenersRecord& dbusInterfaceNameListenersRecord,
                                          const bool& isDBusInterfaceNameAvailable);


    void removeUniqueName(const DBusUniqueNamesMapIterator &_dbusUniqueName, const std::string &_serviceName);
    DBusUniqueNameRecord* insertServiceNameMapping(const std::string& dbusUniqueName, const std::string& dbusServiceName);
    bool findCachedDbusService(const std::string& dbusServiceName, DBusUniqueNameRecord** uniqueNameRecord);
    bool findCachedObjectPath(const std::string& dbusObjectPathName, const DBusUniqueNameRecord* uniqueNameRecord, DBusObjectPathCache* objectPathCache);

    void fetchAllServiceNames();

    inline bool isDBusServiceName(const std::string &_name) {
        return (_name.length() > 0 && _name[0] != ':');
    };


    inline bool isOrgFreedesktopDBusInterface(const std::string& dbusInterfaceName) {
        return dbusInterfaceName.find("org.freedesktop.DBus.") == 0;
    }

    std::thread::id notificationThread_;

    std::unordered_set<std::string> dbusPredefinedServices_;

private:
    typedef std::map<std::shared_ptr<DBusProxyConnection>, std::shared_ptr<DBusServiceRegistry>> RegistryMap_t;
    static std::shared_ptr<RegistryMap_t> getRegistryMap() {
        static std::shared_ptr<RegistryMap_t> registries(new RegistryMap_t);
        return registries;
    }
    static std::mutex registriesMutex_;
    std::shared_ptr<RegistryMap_t> registries_;
    std::shared_ptr<DBusAddressTranslator> translator_;
    std::weak_ptr<DBusServiceRegistry> selfReference_;
};


} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSSERVICEREGISTRY_HPP_
