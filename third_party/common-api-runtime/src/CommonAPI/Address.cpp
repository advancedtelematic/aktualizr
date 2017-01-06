// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sstream>

#include <CommonAPI/Address.hpp>

namespace CommonAPI {

Address::Address(const std::string &_address) {
    // TODO: handle error situation (_address is no valid CommonAPI address)
    setAddress(_address);
}

Address::Address(const std::string &_domain,
                 const std::string &_interface,
                 const std::string &_instance)
    : domain_(_domain),
      interface_(_interface),
      instance_(_instance) {
}

Address::Address(const Address &_source)
    : domain_(_source.domain_),
      interface_(_source.interface_),
      instance_(_source.instance_) {
}

Address::~Address() {
}

bool
Address::operator ==(const Address &_other) const {
    return (domain_ == _other.domain_ &&
            interface_ == _other.interface_ &&
            instance_ == _other.instance_);
}

bool
Address::operator !=(const Address &_other) const {
    return (domain_ != _other.domain_ ||
            interface_ != _other.interface_ ||
            instance_ != _other.instance_);
}

bool
Address::operator<(const Address &_other) const {
    if (domain_ < _other.domain_)
        return true;

    if (domain_ == _other.domain_) {
        if (interface_ < _other.interface_)
            return true;

        if (interface_ == _other.interface_) {
            if (instance_ < _other.instance_)
                return true;
        }
    }

    return false;
}

std::string
Address::getAddress() const {
    return (domain_ + ":" + interface_ + ":" + instance_);
}

void
Address::setAddress(const std::string &_address) {
    std::istringstream addressStream(_address);
    if (std::getline(addressStream, domain_, ':')) {
        if (std::getline(addressStream, interface_, ':')) {
            if(!std::getline(addressStream, instance_, ':')) {
                if(std::getline(addressStream, instance_)) {
                }
            }
        }
    }
}

const std::string &
Address::getDomain() const {
    return domain_;
}

void
Address::setDomain(const std::string &_domain) {
    domain_ = _domain;
}

const std::string &
Address::getInterface() const {
    return interface_;
}

void
Address::setInterface(const std::string &_interface) {
    interface_ = _interface;
}

const std::string &
Address::getInstance() const {
    return instance_;
}

void
Address::setInstance(const std::string &_instance) {
    instance_ = _instance;
}

std::ostream &
operator<<(std::ostream &_out, const Address &_address) {
    _out << _address.domain_
          << ":" << _address.interface_
          << ":" << _address.instance_;
    return _out;
}

} // namespace CommonAPI
