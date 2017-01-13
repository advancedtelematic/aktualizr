// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_ADDRESSTRANSLATOR_HPP_
#define COMMONAPI_DBUS_ADDRESSTRANSLATOR_HPP_

#include <map>
#include <memory>
#include <mutex>

#include <CommonAPI/Types.hpp>
#include <CommonAPI/Address.hpp>
#include <CommonAPI/DBus/DBusAddress.hpp>
#include <CommonAPI/DBus/DBusTypes.hpp>

namespace CommonAPI {
namespace DBus {

class DBusAddressTranslator {
public:
    COMMONAPI_EXPORT static std::shared_ptr<DBusAddressTranslator> get();

    COMMONAPI_EXPORT DBusAddressTranslator();

    COMMONAPI_EXPORT void init();

    COMMONAPI_EXPORT bool translate(const std::string &_key, DBusAddress &_value);
    COMMONAPI_EXPORT bool translate(const CommonAPI::Address &_key, DBusAddress &_value);

    COMMONAPI_EXPORT bool translate(const DBusAddress &_key, std::string &_value);
    COMMONAPI_EXPORT bool translate(const DBusAddress &_key, CommonAPI::Address &_value);

    COMMONAPI_EXPORT void insert(const std::string &_address,
        const std::string &_service, const std::string &_path, const std::string &_interface, const bool _objPathStartWithDigits = false);

    COMMONAPI_EXPORT DBusType_t getDBusBusType(const ConnectionId_t &_connectionId) const ;

    /**
     * @brief Returns whether or not org.freedesktop.DBus.Peer interface is used in a (valid) name mapping.
     * @return true in case any (valid) mapping of org.freedesktop.DBus.Peer is present, otherwise false
     */
    COMMONAPI_EXPORT bool isOrgFreedesktopDBusPeerMapped() const;

private:
    COMMONAPI_EXPORT bool readConfiguration();

    COMMONAPI_EXPORT bool isValid(const std::string &, const char,
                                  bool = false, bool = false, bool = false) const;

private:
    bool isDefault_;

    std::string defaultConfig_;
    std::string defaultDomain_;

    std::map<CommonAPI::Address, DBusAddress> forwards_;
    std::map<DBusAddress, CommonAPI::Address> backwards_;

    std::mutex mutex_;

    std::map<ConnectionId_t, DBusType_t> dbusTypes_;

    bool orgFreedesktopDBusPeerMapped_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_ADDRESSTRANSLATOR_HPP_
