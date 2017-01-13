// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSTYPES_HPP_
#define COMMONAPI_DBUS_DBUSTYPES_HPP_

#include <string>
#include <unordered_map>

#include <dbus/dbus.h>

namespace CommonAPI {
namespace DBus {

typedef std::unordered_map<std::string, bool> DBusPropertiesChangedDict;
typedef std::unordered_map<std::string,
            DBusPropertiesChangedDict> DBusInterfacesAndPropertiesDict;
typedef std::unordered_map<std::string,
            DBusInterfacesAndPropertiesDict> DBusObjectPathAndInterfacesDict;

enum class DBusType_t {
    SESSION = DBUS_BUS_SESSION,
    SYSTEM = DBUS_BUS_SYSTEM,
    STARTER = DBUS_BUS_STARTER,
    WRAPPED
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSTYPES_HPP_
