// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cassert>
#include <vector>

#include <CommonAPI/DBus/DBusFreedesktopPropertiesStub.hpp>
#include <CommonAPI/DBus/DBusStubAdapter.hpp>
#include <CommonAPI/DBus/DBusOutputStream.hpp>
#include <CommonAPI/DBus/DBusInputStream.hpp>

namespace CommonAPI {
namespace DBus {

DBusFreedesktopPropertiesStub::DBusFreedesktopPropertiesStub(
    const std::string &_path, const std::string &_interface,
    const std::shared_ptr<DBusProxyConnection> &_connection,
    const std::shared_ptr<DBusStubAdapter> &_adapter)
    : path_(_path),
      connection_(_connection),
      adapter_(_adapter) {
    assert(!path_.empty());
    assert(path_[0] == '/');
    assert(_connection);

    dbusInterfacesLock_.lock();
    if(managedInterfaces_.find(_interface) == managedInterfaces_.end()) {
        managedInterfaces_.insert({ _interface, _adapter });
    }
    dbusInterfacesLock_.unlock();
}

DBusFreedesktopPropertiesStub::~DBusFreedesktopPropertiesStub() {
    // TODO: Check if there is some deregistration etc. necessary
}

const char* DBusFreedesktopPropertiesStub::getMethodsDBusIntrospectionXmlData() const {
    return "<method name=\"Get\">\n"
             "<arg type=\"s\" name=\"interface_name\" direction=\"in\"/>\n"
             "<arg type=\"s\" name=\"property_name\" direction=\"in\"/>\n"
             "<arg type=\"v\" name=\"value\" direction=\"out\"/>\n"
           "</method>\n"
           "<method name=\"GetAll\">\n"
             "<arg type=\"s\" name=\"interface_name\" direction=\"in\"/>\n"
             "<arg type=\"a{sv}\" name=\"properties\" direction=\"out\"/>\n"
           "</method>\n"
           "<method name=\"Set\">\n"
             "<arg type=\"s\" name=\"interface_name\" direction=\"in\"/>\n"
             "<arg type=\"s\" name=\"property_name\" direction=\"in\"/>\n"
             "<arg type=\"v\" name=\"value\" direction=\"in\"/>\n"
           "</method>\n"
           "<signal name=\"PropertiesChanged\">\n"
             "<arg type=\"s\" name=\"interface_name\"/>\n"
             "<arg type=\"a{sv}\" name=\"changed_properties\"/>\n"
             "<arg type=\"as\" name=\"invalidated_properties\"/>\n"
           "</signal>\n";
}

bool
DBusFreedesktopPropertiesStub::onInterfaceDBusMessage(const DBusMessage &_message) {
    auto connection = connection_.lock();
    if (!connection || !connection->isConnected()) {
        return false;
    }

    if (!_message.isMethodCallType() ||
        !(_message.hasMemberName("Get") ||
        _message.hasMemberName("GetAll") ||
        _message.hasMemberName("Set"))) {
        return false;
    }

    std::string interface;
    DBusInputStream input(_message);
    input >> interface;
    if(input.hasError()) {
        return false;
    }

    std::lock_guard<std::mutex> itsLock(dbusInterfacesLock_);

    auto it = managedInterfaces_.find(interface);
    if(it == managedInterfaces_.end()) {
        return false;
    }

    return it->second->onInterfaceDBusFreedesktopPropertiesMessage(_message);
}

bool DBusFreedesktopPropertiesStub::hasFreedesktopProperties() {
    return false;
}

const std::string &DBusFreedesktopPropertiesStub::getObjectPath() const {
    return path_;
}

const std::string &DBusFreedesktopPropertiesStub::getInterface() {
    static std::string theInterface("org.freedesktop.DBus.Properties");
    return theInterface;
}

} // namespace DBus
} // namespace CommonAPI
