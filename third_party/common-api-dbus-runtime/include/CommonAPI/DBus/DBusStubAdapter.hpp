// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSSTUBADAPTER_HPP_
#define COMMONAPI_DBUS_DBUSSTUBADAPTER_HPP_

#include <memory>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Stub.hpp>
#include <CommonAPI/DBus/DBusAddress.hpp>
#include <CommonAPI/DBus/DBusInterfaceHandler.hpp>

namespace CommonAPI {
namespace DBus {

class DBusProxyConnection;

class DBusStubAdapter
        : virtual public CommonAPI::StubAdapter,
          public DBusInterfaceHandler {
 public:
     COMMONAPI_EXPORT DBusStubAdapter(const DBusAddress &_dbusAddress,
                    const std::shared_ptr<DBusProxyConnection> &_connection,
                    const bool isManagingInterface);

     COMMONAPI_EXPORT virtual ~DBusStubAdapter();

     COMMONAPI_EXPORT virtual void init(std::shared_ptr<DBusStubAdapter> _instance);
     COMMONAPI_EXPORT virtual void deinit();

     COMMONAPI_EXPORT const DBusAddress &getDBusAddress() const;
     COMMONAPI_EXPORT const std::shared_ptr<DBusProxyConnection> &getDBusConnection() const;

     COMMONAPI_EXPORT bool isManaging() const;

     COMMONAPI_EXPORT virtual const char* getMethodsDBusIntrospectionXmlData() const = 0;
     COMMONAPI_EXPORT virtual bool onInterfaceDBusMessage(const DBusMessage &_message) = 0;

     COMMONAPI_EXPORT virtual void deactivateManagedInstances() = 0;
     COMMONAPI_EXPORT virtual bool hasFreedesktopProperties();
     COMMONAPI_EXPORT virtual bool onInterfaceDBusFreedesktopPropertiesMessage(const DBusMessage &_message) = 0;

 protected:
    DBusAddress dbusAddress_;
    const std::shared_ptr<DBusProxyConnection> connection_;
    const bool isManaging_;
};

} // namespace dbus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSSTUBADAPTER_HPP_
