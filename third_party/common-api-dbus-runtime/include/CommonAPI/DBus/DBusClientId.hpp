// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSCLIENTID_HPP_
#define COMMONAPI_DBUS_DBUSCLIENTID_HPP_

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Types.hpp>

namespace CommonAPI {
namespace DBus {

class DBusMessage;

/**
 * \brief Implementation of CommonAPI::ClientId for DBus
 *
 * This class represents the DBus specific implementation of CommonAPI::ClientId.
 * It internally uses a string to identify clients. This string is the unique sender id used by dbus.
 */
class DBusClientId
        : public CommonAPI::ClientId {
    friend struct std::hash<DBusClientId>;

public:
    COMMONAPI_EXPORT DBusClientId(std::string dbusId);

    COMMONAPI_EXPORT bool operator==(CommonAPI::ClientId& clientIdToCompare);
    COMMONAPI_EXPORT bool operator==(DBusClientId& clientIdToCompare);
    COMMONAPI_EXPORT size_t hashCode();

    COMMONAPI_EXPORT const char * getDBusId();

    COMMONAPI_EXPORT DBusMessage createMessage(const std::string objectPath, const std::string interfaceName, const std::string signalName) const;
protected:
    std::string dbusId_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // DBUSCLIENTID_HPP_
