// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSOBJECTMANAGER_HPP_
#define COMMONAPI_DBUS_DBUSOBJECTMANAGER_HPP_

#include <CommonAPI/DBus/DBusProxyConnection.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>
#include <CommonAPI/DBus/DBusObjectManagerStub.hpp>

namespace CommonAPI {
namespace DBus {

class DBusStubAdapter;
class DBusInterfaceHandler;

class DBusObjectManager {
 public:
     COMMONAPI_EXPORT DBusObjectManager(const std::shared_ptr<DBusProxyConnection>&);
     COMMONAPI_EXPORT ~DBusObjectManager();

     COMMONAPI_EXPORT bool registerDBusStubAdapter(std::shared_ptr<DBusStubAdapter> dbusStubAdapter);
     COMMONAPI_EXPORT bool unregisterDBusStubAdapter(std::shared_ptr<DBusStubAdapter> dbusStubAdapter);

    //Zusammenfassbar mit "registerDBusStubAdapter"?
     COMMONAPI_EXPORT bool exportManagedDBusStubAdapter(const std::string& parentObjectPath, std::shared_ptr<DBusStubAdapter> dbusStubAdapter);
     COMMONAPI_EXPORT bool unexportManagedDBusStubAdapter(const std::string& parentObjectPath, std::shared_ptr<DBusStubAdapter> dbusStubAdapter);

     COMMONAPI_EXPORT bool handleMessage(const DBusMessage&);

     COMMONAPI_EXPORT std::shared_ptr<DBusObjectManagerStub> getRootDBusObjectManagerStub();

 private:
    // objectPath, interfaceName
    typedef std::pair<std::string, std::string> DBusInterfaceHandlerPath;

    COMMONAPI_EXPORT bool addDBusInterfaceHandler(const DBusInterfaceHandlerPath& dbusInterfaceHandlerPath,
                                 std::shared_ptr<DBusInterfaceHandler> dbusInterfaceHandler);

    COMMONAPI_EXPORT bool removeDBusInterfaceHandler(const DBusInterfaceHandlerPath& dbusInterfaceHandlerPath,
                                    std::shared_ptr<DBusInterfaceHandler> dbusInterfaceHandler);

    COMMONAPI_EXPORT bool onIntrospectableInterfaceDBusMessage(const DBusMessage& callMessage);
    COMMONAPI_EXPORT bool onFreedesktopPropertiesDBusMessage(const DBusMessage& callMessage);


    typedef std::unordered_map<DBusInterfaceHandlerPath, std::shared_ptr<DBusInterfaceHandler>> DBusRegisteredObjectsTable;
    DBusRegisteredObjectsTable dbusRegisteredObjectsTable_;

    std::shared_ptr<DBusObjectManagerStub> rootDBusObjectManagerStub_;

    typedef std::pair<std::shared_ptr<DBusObjectManagerStub>, uint32_t> ReferenceCountedDBusObjectManagerStub;
    typedef std::unordered_map<std::string, ReferenceCountedDBusObjectManagerStub> RegisteredObjectManagersTable;
    RegisteredObjectManagersTable managerStubs_;

    std::weak_ptr<DBusProxyConnection> dbusConnection_;
    std::recursive_mutex objectPathLock_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSOBJECTMANAGER_HPP_
