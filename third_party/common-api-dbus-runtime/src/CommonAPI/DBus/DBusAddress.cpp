// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>

#include <CommonAPI/DBus/DBusAddress.hpp>

namespace CommonAPI {
namespace DBus {

DBusAddress::DBusAddress(const std::string &_service,
                         const std::string &_objectPath,
                         const std::string &_interface)
    : service_(_service),
      objectPath_(_objectPath),
      interface_(_interface) {
}

DBusAddress::DBusAddress(const DBusAddress &_source)
    : service_(_source.service_),
      objectPath_(_source.objectPath_),
      interface_(_source.interface_) {
}

DBusAddress::~DBusAddress() {
}

bool
DBusAddress::operator==(const DBusAddress &_other) const {
    return (service_ == _other.service_ &&
            objectPath_ == _other.objectPath_ &&
            interface_ == _other.interface_);
}

bool
DBusAddress::operator !=(const DBusAddress &_other) const {
    return (service_ != _other.service_ ||
            objectPath_ != _other.objectPath_ ||
            interface_ != _other.interface_);
}

bool
DBusAddress::operator<(const DBusAddress &_other) const {
    if (service_ < _other.service_)
        return true;

    if (service_ == _other.service_) {
        if (objectPath_ < _other.objectPath_)
            return true;

        if (objectPath_ == _other.objectPath_) {
            if (interface_ < _other.interface_)
                return true;
        }
    }

    return false;
}

const std::string &
DBusAddress::getService() const {
    return service_;
}

void
DBusAddress::setService(const std::string &_service) {
    service_ = _service;
}

const std::string &
DBusAddress::getObjectPath() const {
    return objectPath_;
}

void
DBusAddress::setObjectPath(const std::string &_objectPath) {
    objectPath_ = _objectPath;
}

const std::string &
DBusAddress::getInterface() const {
    return interface_;
}

void
DBusAddress::setInterface(const std::string &_interface) {
    interface_ = _interface;
}

std::ostream &
operator<<(std::ostream &_out, const DBusAddress &_dbusAddress) {
    _out << "service=" << _dbusAddress.service_.c_str()
        << ":path=" << _dbusAddress.objectPath_.c_str()
        << ":interface=" << _dbusAddress.interface_.c_str();
    return _out;
}


} // namespace DBus
} // namespace CommonAPI
