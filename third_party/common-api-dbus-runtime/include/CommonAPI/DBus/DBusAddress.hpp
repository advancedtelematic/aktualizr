// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_ADDRESS_HPP_
#define COMMONAPI_DBUS_ADDRESS_HPP_

#include <iostream>
#include <map>

#include <CommonAPI/Export.hpp>

namespace CommonAPI {
namespace DBus {

class DBusAddress {
public:
    COMMONAPI_EXPORT DBusAddress(const std::string &_service = "",
                const std::string &_objectPath = "",
                const std::string &_interface = "");
    COMMONAPI_EXPORT DBusAddress(const DBusAddress &_source);
    COMMONAPI_EXPORT virtual ~DBusAddress();

    COMMONAPI_EXPORT bool operator==(const DBusAddress &_other) const;
    COMMONAPI_EXPORT bool operator!=(const DBusAddress &_other) const;
    COMMONAPI_EXPORT bool operator<(const DBusAddress &_other) const;

    COMMONAPI_EXPORT const std::string &getInterface() const;
    COMMONAPI_EXPORT void setInterface(const std::string &_interface);

    COMMONAPI_EXPORT const std::string &getObjectPath() const;
    COMMONAPI_EXPORT void setObjectPath(const std::string &_objectPath);

    COMMONAPI_EXPORT const std::string &getService() const;
    COMMONAPI_EXPORT void setService(const std::string &_service);

private:
    std::string service_;
    std::string objectPath_;
    std::string interface_;

friend std::ostream &operator<<(std::ostream &_out, const DBusAddress &_dbusAddress);
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_ADDRESS_HPP_
