// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <typeinfo>

#include <CommonAPI/DBus/DBusClientId.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>

namespace std {

template<>
struct hash<CommonAPI::DBus::DBusClientId> {
public:
    size_t operator()(CommonAPI::DBus::DBusClientId* dbusClientIdToHash) const {
        return (hash<string>()(dbusClientIdToHash->dbusId_));
    }
};

} /* namespace std */

namespace CommonAPI {
namespace DBus {

DBusClientId::DBusClientId(std::string dbusId) :
                dbusId_(dbusId) {
}

bool DBusClientId::operator==(CommonAPI::ClientId& clientIdToCompare) {
    try {
        DBusClientId clientIdToCompareDBus = DBusClientId(dynamic_cast<DBusClientId&>(clientIdToCompare));
        return (clientIdToCompareDBus == *this);
    }
    catch (...) {
        return false;
    }
}

bool DBusClientId::operator==(DBusClientId& clientIdToCompare) {
    return (clientIdToCompare.dbusId_ == dbusId_);
}

size_t DBusClientId::hashCode()
{
    return std::hash<DBusClientId>()(this);
}

const char * DBusClientId::getDBusId() {
    return dbusId_.c_str();
}

DBusMessage DBusClientId::createMessage(const std::string objectPath, const std::string interfaceName, const std::string signalName) const
{
    DBusMessage returnMessage = DBusMessage::createSignal(objectPath, interfaceName, signalName);
    returnMessage.setDestination(dbusId_.c_str());

    return(returnMessage);
}

} // namespace DBus
} // namespace CommonAPI
