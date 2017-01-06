// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/DBus/DBusAddress.hpp>
#include <CommonAPI/DBus/DBusAddressTranslator.hpp>
#include <CommonAPI/DBus/DBusProxyBase.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>

namespace CommonAPI {
namespace DBus {

DBusProxyBase::DBusProxyBase(
    const DBusAddress &_dbusAddress,
    const std::shared_ptr<DBusProxyConnection> &_connection)
    : dbusAddress_(_dbusAddress),
      connection_(_connection) {
    DBusAddressTranslator::get()->translate(dbusAddress_, address_);
}

DBusMessage
DBusProxyBase::createMethodCall(const std::string &_method, const std::string &_signature) const {
    return DBusMessage::createMethodCall(getDBusAddress(), _method, _signature);
}


const DBusAddress &
DBusProxyBase::getDBusAddress() const {
    return dbusAddress_;
}

const std::shared_ptr<DBusProxyConnection> &
DBusProxyBase::getDBusConnection() const {
    return connection_;
}

DBusProxyConnection::DBusSignalHandlerToken DBusProxyBase::addSignalMemberHandler(
                const std::string& objectPath,
                const std::string& interfaceName,
                const std::string& signalName,
                const std::string& signalSignature,
                DBusProxyConnection::DBusSignalHandler* dbusSignalHandler,
                const bool justAddFilter) {
    return connection_->addSignalMemberHandler(
                objectPath,
                interfaceName,
                signalName,
                signalSignature,
                dbusSignalHandler,
                justAddFilter);
}

DBusProxyConnection::DBusSignalHandlerToken DBusProxyBase::addSignalMemberHandler(
                const std::string &objectPath,
                const std::string &interfaceName,
                const std::string &signalName,
                const std::string &signalSignature,
                const std::string &getMethodName,
                DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
                const bool justAddFilter) {
    (void)getMethodName;
    return addSignalMemberHandler(
                objectPath,
                interfaceName,
                signalName,
                signalSignature,
                dbusSignalHandler,
                justAddFilter);
}

bool DBusProxyBase::removeSignalMemberHandler(const DBusProxyConnection::DBusSignalHandlerToken& _dbusSignalHandlerToken, const DBusProxyConnection::DBusSignalHandler* _dbusSignalHandler) {
    return connection_->removeSignalMemberHandler(_dbusSignalHandlerToken, _dbusSignalHandler);
}

} // namespace DBus
} // namespace CommonAPI
