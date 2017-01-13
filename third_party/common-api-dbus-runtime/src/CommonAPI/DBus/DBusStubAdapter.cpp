// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/DBus/DBusAddressTranslator.hpp>
#include <CommonAPI/DBus/DBusStubAdapter.hpp>

namespace CommonAPI {
namespace DBus {

DBusStubAdapter::DBusStubAdapter(const DBusAddress &_dbusAddress,
                                 const std::shared_ptr<DBusProxyConnection> &_connection,
                                 const bool _isManaging)
    : dbusAddress_(_dbusAddress),
      connection_(_connection),
      isManaging_(_isManaging) {
}

DBusStubAdapter::~DBusStubAdapter() {
    deinit();
}

void DBusStubAdapter::init(std::shared_ptr<DBusStubAdapter> _instance) {
    (void)_instance;
    DBusAddressTranslator::get()->translate(dbusAddress_, address_);
}

void DBusStubAdapter::deinit() {
}

const DBusAddress &DBusStubAdapter::getDBusAddress() const {
    return dbusAddress_;
}

const std::shared_ptr<DBusProxyConnection> &DBusStubAdapter::getDBusConnection() const {
    return connection_;
}

bool DBusStubAdapter::isManaging() const {
    return isManaging_;
}

bool DBusStubAdapter::hasFreedesktopProperties() {
    return false;
}

} // namespace DBus
} // namespace CommonAPI
