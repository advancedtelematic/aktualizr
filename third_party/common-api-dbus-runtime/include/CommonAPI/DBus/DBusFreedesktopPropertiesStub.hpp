// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUS_FREEDESKTOPPROPERTIESSTUB_HPP_
#define COMMONAPI_DBUS_DBUS_FREEDESKTOPPROPERTIESSTUB_HPP_

#include <memory>
#include <mutex>
#include <string>

#include <CommonAPI/DBus/DBusInterfaceHandler.hpp>

namespace CommonAPI {
namespace DBus {

class DBusStubAdapter;

/**
 * Stub for standard <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-properties">org.freedesktop.dbus.Properties</a> interface.
 *
 * DBusFreedesktopPropertiesStub gets the DBusStubAdapter for handling the actual properties with instantiation.
 */
class DBusFreedesktopPropertiesStub: public DBusInterfaceHandler {
public:
    DBusFreedesktopPropertiesStub(const std::string &_path,
                                  const std::string &_interface,
                                  const std::shared_ptr<DBusProxyConnection> &_connection,
                                  const std::shared_ptr<DBusStubAdapter> &_adapter);

    virtual ~DBusFreedesktopPropertiesStub();

    const std::string &getObjectPath() const;
    static const std::string &getInterface();

    virtual const char* getMethodsDBusIntrospectionXmlData() const;
    virtual bool onInterfaceDBusMessage(const DBusMessage &_message);
    virtual bool hasFreedesktopProperties();

private:
    std::string path_;
    std::weak_ptr<DBusProxyConnection> connection_;
    std::shared_ptr<DBusStubAdapter> adapter_;

    typedef std::unordered_map<std::string, std::shared_ptr<DBusStubAdapter>> DBusInterfacesMap;
    DBusInterfacesMap managedInterfaces_;

    std::mutex dbusInterfacesLock_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUS_FREEDESKTOPPROPERTIESSTUB_HPP_
