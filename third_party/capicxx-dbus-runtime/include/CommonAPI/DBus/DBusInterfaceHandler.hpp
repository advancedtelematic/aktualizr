// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_DBUS_DBUSINTERFACEHANDLER_HPP_
#define COMMONAPI_DBUS_DBUSINTERFACEHANDLER_HPP_

#include <memory>

#include <CommonAPI/DBus/DBusProxyConnection.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>

namespace CommonAPI {
namespace DBus {

class DBusInterfaceHandler {
 public:
    virtual ~DBusInterfaceHandler() {}

    virtual const char* getMethodsDBusIntrospectionXmlData() const = 0;

    virtual bool onInterfaceDBusMessage(const DBusMessage& dbusMessage) = 0;

    virtual bool hasFreedesktopProperties() = 0;
};

} // namespace dbus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSINTERFACEHANDLER_HPP_
