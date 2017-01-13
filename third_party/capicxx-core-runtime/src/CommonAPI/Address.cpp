// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cctype>
#include <sstream>

#include <CommonAPI/Address.hpp>
#include <CommonAPI/Logger.hpp>

namespace CommonAPI {

Address::Address() {

}

Address::Address(const std::string &_address) {
    // TODO: handle error situation (_address is no valid CommonAPI address)
    setAddress(_address);
}

Address::Address(const std::string &_domain,
                 const std::string &_interface,
                 const std::string &_instance) {
    setAddress(_domain + ":" + _interface + ":" + _instance);
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
    std::string itsDomain, itsInterface, itsVersion, itsInstance;
    std::size_t itsDomainPos, itsInterfacePos, itsVersionPos, itsInstancePos;
    bool isValid(true);

    itsDomainPos = _address.find(':');
    if (itsDomainPos == std::string::npos) {
        isValid = false;
    }

    if (isValid) {
        itsDomain = _address.substr(0, itsDomainPos);
        itsDomainPos++;

        itsInterfacePos = _address.find(':', itsDomainPos);
        if (itsInterfacePos == std::string::npos) {
            isValid = false;
        }
    }

    if (isValid) {
        itsInterface = _address.substr(itsDomainPos, itsInterfacePos-itsDomainPos);
        itsInterfacePos++;

        itsVersionPos = _address.find(':', itsInterfacePos);
        if (itsVersionPos == std::string::npos) {
            itsInstance = _address.substr(itsInterfacePos);
        } else {
            itsVersion = _address.substr(itsInterfacePos, itsVersionPos-itsInterfacePos);
            if (itsVersion != "") {
                // check version
                std::size_t itsSeparatorPos = itsVersion.find('_');
                if (itsSeparatorPos == std::string::npos) {
                    isValid = false;
                }

                if (isValid) {
                    if( *(itsVersion.begin()) != 'v')
                        isValid = false;
                    if(isValid) {
                        for (auto it = itsVersion.begin()+1; it != itsVersion.end(); ++it) {
                            if (!isdigit(*it) && *it != '_') {
                                isValid = false;
                                break;
                            }
                        }
                    }
                }
            }
            itsVersionPos++;

            if (isValid) {
                itsInstancePos = _address.find(':', itsVersionPos);
                if (itsInstancePos != std::string::npos) {
                    isValid = false;
                }

                itsInstance = _address.substr(itsVersionPos);
            }
        }
    }

    if (isValid) {
        domain_ = itsDomain;
        if (itsVersion != "")
            itsInterface += ":" + itsVersion;
        else
            itsInterface += ":v1_0";
        interface_ = itsInterface;
        instance_ = itsInstance;
    } else {
        COMMONAPI_ERROR("Attempted to set invalid CommonAPI address: ", _address);
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
