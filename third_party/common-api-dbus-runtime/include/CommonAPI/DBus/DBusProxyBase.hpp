// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSPROXYBASE_HPP_
#define COMMONAPI_DBUS_DBUSPROXYBASE_HPP_

#include <functional>
#include <memory>
#include <string>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Proxy.hpp>
#include <CommonAPI/Types.hpp>

#include <CommonAPI/DBus/DBusAddress.hpp>
#include <CommonAPI/DBus/DBusProxyConnection.hpp>

namespace CommonAPI {
namespace DBus {

class DBusAddress;

class DBusProxyBase
        : public virtual CommonAPI::Proxy {
public:
    COMMONAPI_EXPORT DBusProxyBase(const DBusAddress &_address,
                  const std::shared_ptr<DBusProxyConnection> &_connection);
    COMMONAPI_EXPORT virtual ~DBusProxyBase() {}

    COMMONAPI_EXPORT const DBusAddress &getDBusAddress() const;
    COMMONAPI_EXPORT const std::shared_ptr<DBusProxyConnection> &getDBusConnection() const;

    COMMONAPI_EXPORT DBusMessage createMethodCall(const std::string &_method,
                                 const std::string &_signature = "") const;

    COMMONAPI_EXPORT virtual DBusProxyConnection::DBusSignalHandlerToken addSignalMemberHandler(
            const std::string &objectPath,
            const std::string &interfaceName,
            const std::string &signalName,
            const std::string &signalSignature,
            DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
            const bool justAddFilter);

    COMMONAPI_EXPORT virtual DBusProxyConnection::DBusSignalHandlerToken addSignalMemberHandler(
            const std::string &objectPath,
            const std::string &interfaceName,
            const std::string &signalName,
            const std::string &signalSignature,
            const std::string &getMethodName,
            DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
            const bool justAddFilter);

    COMMONAPI_EXPORT virtual bool removeSignalMemberHandler(
            const DBusProxyConnection::DBusSignalHandlerToken &_dbusSignalHandlerToken,
            const DBusProxyConnection::DBusSignalHandler *_dbusSignalHandler = NULL);

    COMMONAPI_EXPORT virtual void getCurrentValueForSignalListener(
            const std::string &_getMethodName,
            DBusProxyConnection::DBusSignalHandler *_handler,
            const uint32_t _subscription) {
        (void)_getMethodName;
        (void)_handler;
        (void)_subscription;
    }

    COMMONAPI_EXPORT virtual void init() = 0;

 protected:
    COMMONAPI_EXPORT DBusProxyBase(const DBusProxyBase &) = delete;

    DBusAddress dbusAddress_;
    std::shared_ptr<DBusProxyConnection> connection_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSPROXYBASE_HPP_
