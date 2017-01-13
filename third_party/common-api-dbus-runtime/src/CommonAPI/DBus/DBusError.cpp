// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cassert>
#include <cstring>

#include <CommonAPI/DBus/DBusError.hpp>

namespace CommonAPI {
namespace DBus {

DBusError::DBusError() {
    dbus_error_init(&libdbusError_);
}

DBusError::~DBusError() {
    dbus_error_free(&libdbusError_);
}

DBusError::operator bool() const {
    return 0 != dbus_error_is_set(&libdbusError_);
}

void DBusError::clear() {
    dbus_error_free(&libdbusError_);
}

std::string DBusError::getName() const {

    return std::string(libdbusError_.name);
}

std::string DBusError::getMessage() const {

    return std::string(libdbusError_.message);
}

} // namespace DBus
} // namespace CommonAPI
