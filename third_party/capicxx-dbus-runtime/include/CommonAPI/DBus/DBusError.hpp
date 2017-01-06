// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUS_ERROR_HPP_
#define COMMONAPI_DBUS_DBUS_ERROR_HPP_

#include <string>
#include <dbus/dbus.h>

#include <CommonAPI/Export.hpp>

namespace CommonAPI {
namespace DBus {

class DBusConnection;


class COMMONAPI_EXPORT DBusError {
 public:
    DBusError();
    ~DBusError();

    operator bool() const;

    void clear();

    std::string getName() const;
    std::string getMessage() const;

 private:
    ::DBusError libdbusError_;

    friend class DBusConnection;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUS_ERROR_HPP_
