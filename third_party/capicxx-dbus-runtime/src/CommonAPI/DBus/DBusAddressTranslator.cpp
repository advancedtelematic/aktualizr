// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <sys/stat.h>

#include <algorithm>

#include <CommonAPI/IniFileReader.hpp>
#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Runtime.hpp>
#include <CommonAPI/DBus/DBusAddressTranslator.hpp>

namespace CommonAPI {
namespace DBus {

const char *COMMONAPI_DBUS_DEFAULT_CONFIG_FILE = "commonapi-dbus.ini";
const char *COMMONAPI_DBUS_DEFAULT_CONFIG_FOLDER = "/etc/";


std::shared_ptr<DBusAddressTranslator> DBusAddressTranslator::get() {
    static std::shared_ptr<DBusAddressTranslator> theTranslator = std::make_shared<DBusAddressTranslator>();
    return theTranslator;
}

DBusAddressTranslator::DBusAddressTranslator()
    : defaultDomain_("local"), orgFreedesktopDBusPeerMapped_(false) {
    init();

    isDefault_ = ("dbus" == Runtime::get()->getDefaultBinding());
}

void
DBusAddressTranslator::init() {
    // Determine default configuration file
    const char *config = getenv("COMMONAPI_DBUS_CONFIG");
    if (config) {
        defaultConfig_ = config;
    } else {
        defaultConfig_ = COMMONAPI_DBUS_DEFAULT_CONFIG_FOLDER;
        defaultConfig_ += "/";
        defaultConfig_ += COMMONAPI_DBUS_DEFAULT_CONFIG_FILE;
    }

    (void)readConfiguration();
}

bool
DBusAddressTranslator::translate(const std::string &_key, DBusAddress &_value) {
    return translate(CommonAPI::Address(_key), _value);
}

bool
DBusAddressTranslator::translate(const CommonAPI::Address &_key, DBusAddress &_value) {
    bool result(true);
    mutex_.lock();

    std::string capiInterfaceName = _key.getInterface();

    std::size_t itsVersionPos = capiInterfaceName.rfind(":");
    if( itsVersionPos != std::string::npos) {
        capiInterfaceName = capiInterfaceName.substr(0, itsVersionPos);

        CommonAPI::Address itsCapiAddress(_key);
        itsCapiAddress.setInterface(capiInterfaceName);
        auto it = unversioned_.find(itsCapiAddress);
        if(it != unversioned_.end()) {
            mutex_.unlock();
            insert(_key.getAddress(), std::get<0>(it->second), std::get<1>(it->second), std::get<2>(it->second));
            mutex_.lock();
            unversioned_.erase(itsCapiAddress);
        }
    }

    const auto it = forwards_.find(_key);
    if (it != forwards_.end()) {
        _value = it->second;
    } else if (isDefault_) {
        std::string interfaceName(_key.getInterface());
        std::replace(interfaceName.begin(), interfaceName.end(), ':', '.');
        std::string objectPath("/" + _key.getInstance());
        std::replace(objectPath.begin(), objectPath.end(), '.', '/');
        std::string service(interfaceName + "_" + _key.getInstance());

        if (isValid(service, '.', false, false, true)
         && isValid(objectPath, '/', true)
         && isValid(interfaceName, '.')) {

            // check if interface needs to be compatible to newer/older version
            auto it = compatibility_.find(_key.getInterface());
            if(it != compatibility_.end()) {
                _value.setInterface(it->second);
                _value.setService(it->second + "_" + _key.getInstance());
            } else {
                _value.setInterface(interfaceName);
                _value.setService(service);
            }

            _value.setObjectPath(objectPath);

            forwards_.insert({ _key, _value });
            backwards_.insert({ _value, _key });
        }
        else {
            COMMONAPI_ERROR(
                "Translation from CommonAPI address to DBus address failed!");
            result = false;
        }
    } else {
        result = false;
    }

    mutex_.unlock();
    return result;
}

bool
DBusAddressTranslator::translate(const DBusAddress &_key, std::string &_value) {
    CommonAPI::Address address;
    if (translate(_key, address)) {
        _value = address.getAddress();
        return true;
    }
    return false;
}

bool
DBusAddressTranslator::translate(const DBusAddress &_key, CommonAPI::Address &_value) {
    bool result(true);
    std::size_t itsInterfacePos;
    std::string itsVersion;
    std::lock_guard<std::mutex> itsLock(mutex_);

    const auto it = backwards_.find(_key);
    if (it != backwards_.end()) {
        _value = it->second;
    } else if (isDefault_) {
        if (isValid(_key.getObjectPath(), '/', true) && isValid(_key.getInterface(), '.')) {
            std::string interfaceName(_key.getInterface());
            itsInterfacePos = interfaceName.rfind('.');
            itsInterfacePos++;
            if( itsInterfacePos != std::string::npos
                    && ( interfaceName.length() - itsInterfacePos >= 4) ) {
                itsVersion = interfaceName.substr(itsInterfacePos);
                if(isValidVersion(itsVersion)) {
                    interfaceName.replace(itsInterfacePos - 1, 1, ":");
                }
            }
            std::string instance(_key.getObjectPath().substr(1));
            std::replace(instance.begin(), instance.end(), '/', '.');

            _value.setDomain(defaultDomain_);
            _value.setInterface(interfaceName);
            _value.setInstance(instance);

            std::string service = _key.getService();
            if(isValid(service, '.',
                       (service.length() > 0 && service[0] == ':'),
                       (service.length() > 0 && service[0] == ':'),
                       true)) {
                forwards_.insert({_value, _key});
                backwards_.insert({_key, _value});
            }
        } else {
            result = false;
        }
    } else {
        result = false;
    }

    return result;
}

void
DBusAddressTranslator::insert(
        const std::string &_address,
        const std::string &_service, const std::string &_path, const std::string &_interface, const bool _objPathStartWithDigits) {

    if (isValid(_service, '.',
            (_service.length() > 0 && _service[0] == ':'),
            (_service.length() > 0 && _service[0] == ':'),
            true)
      && isValid(_path, '/', true, _objPathStartWithDigits)
      && isValid(_interface, '.')) {
        CommonAPI::Address address(_address);
        DBusAddress dbusAddress(_service, _path, _interface);

        std::lock_guard<std::mutex> itsLock(mutex_);
        auto fw = forwards_.find(address);
        auto bw = backwards_.find(dbusAddress);
        if (fw == forwards_.end() && bw == backwards_.end()) {
            forwards_[address] = dbusAddress;
            backwards_[dbusAddress] = address;
            COMMONAPI_DEBUG(
                "Added address mapping: ", address, " <--> ", dbusAddress);
            if (!orgFreedesktopDBusPeerMapped_) {
                orgFreedesktopDBusPeerMapped_ = (_interface == "org.freedesktop.DBus.Peer");
                if (orgFreedesktopDBusPeerMapped_) {
                    COMMONAPI_DEBUG("org.freedesktop.DBus.Peer mapped");
                }
            }
        } else if(bw != backwards_.end() && bw->second != address) {
            COMMONAPI_ERROR("Trying to overwrite existing DBus address "
                    "which is already mapped to a CommonAPI address: ",
                    dbusAddress, " <--> ", _address);
        } else if(fw != forwards_.end() && fw->second != dbusAddress) {
            COMMONAPI_ERROR("Trying to overwrite existing CommonAPI address "
                    "which is already mapped to a DBus address: ",
                    _address, " <--> ", dbusAddress);
        }
    }
}

bool
DBusAddressTranslator::readConfiguration() {
#define MAX_PATH_LEN 255
    std::string config;
    char currentDirectory[MAX_PATH_LEN];
#ifdef WIN32
    if (GetCurrentDirectory(MAX_PATH_LEN, currentDirectory)) {
#else
    if (getcwd(currentDirectory, MAX_PATH_LEN)) {
#endif
        config = currentDirectory;
        config += "/";
        config += COMMONAPI_DBUS_DEFAULT_CONFIG_FILE;

        struct stat s;
        if (stat(config.c_str(), &s) != 0) {
            config = defaultConfig_;
        }
    }

    IniFileReader reader;
    if (!reader.load(config))
        return false;

    for (auto itsMapping : reader.getSections()) {
        if(itsMapping.first == "segments") {
            std::map<std::string, std::string> mappings = itsMapping.second->getMappings();
            ConnectionId_t connectionId;
            std::string busType;
            for(auto const &it : mappings) {
                connectionId = it.first;
                busType = it.second;
                if(busType == "SESSION") {
                    dbusTypes_.insert({ connectionId, DBusType_t::SESSION });
                } else if (busType == "SYSTEM") {
                    dbusTypes_.insert({ connectionId, DBusType_t::SYSTEM });
                } else {
                    COMMONAPI_FATAL("Invalid bus type specified in .ini file, "
                            "choose one of {SYSTEM, SESSION}");
                    continue;
                }
                COMMONAPI_INFO("D-Bus bus type for connection: " + connectionId +
                        " is set to: " + busType + " via ini file");
            }
            continue;
        } else if(itsMapping.first == "compatibility") {
            // section that defines the compatibility of interfaces
            // CAPI interfaces can be converted to newer/older versions
            std::map<std::string, std::string> mappings = itsMapping.second->getMappings();
            std::string itsInterface;
            std::string itsNewInterface;
            for(auto const &it : mappings) {
                itsInterface = it.first;
                itsNewInterface = it.second;

                if(itsNewInterface.empty()) {
                    // convert to unversioned interface
                    std::size_t itsVersionPos = itsInterface.rfind(":");
                    itsVersionPos++;
                    bool isValid = false;
                    if( itsVersionPos != std::string::npos
                            && ( itsInterface.length() - itsVersionPos >= 4) ) {
                        std::string itsVersion = itsInterface.substr(itsVersionPos);
                        if(isValidVersion(itsVersion)) {
                            itsNewInterface = itsInterface.substr(0, itsVersionPos - 1);
                            isValid = true;
                        }
                    }

                    if(!isValid) {
                        COMMONAPI_INFO("Managed Interface " + itsInterface +
                                " contains no valid version information for compatibility conversion");
                    }
                }
                compatibility_[itsInterface] = itsNewInterface;
            }
            continue;
        }

        CommonAPI::Address itsAddress(itsMapping.first);

        const std::string& service = itsMapping.second->getValue("service");
        const std::string& path = itsMapping.second->getValue("path");

        // check whether CommonAPI::Address "automatically" added "missing" version, thus ":v1_0"
        if(itsMapping.first.rfind(itsAddress.getInterface()) == std::string::npos) {
            std::string capiInterfaceName = itsAddress.getInterface();
            std::size_t itsVersionPos = capiInterfaceName.rfind(":");
            capiInterfaceName = capiInterfaceName.substr(0, itsVersionPos);
            itsAddress.setInterface(capiInterfaceName);
            unversioned_[itsAddress] = std::make_tuple(service, path, itsMapping.second->getValue("interface"));
        } else {
            insert(itsMapping.first, service, path, itsMapping.second->getValue("interface"));
        }
    }

    return true;
}

bool
DBusAddressTranslator::isValid(
        const std::string &_name, const char _separator,
        bool _ignoreFirst, bool _isAllowedToStartWithDigit, bool _isBusName) const {
    (void)_isAllowedToStartWithDigit;
    // DBus addresses must contain at least one separator
    std::size_t separatorPos = _name.find(_separator);
    if (separatorPos == std::string::npos) {
        COMMONAPI_ERROR(
            "Invalid name \'", _name,
            "\'. Contains no \'", _separator, "\'");
        return false;
    }

    bool isInitial(true);
    std::size_t start(0);

    if (_ignoreFirst) {
        start = 1;
        if (separatorPos == 0) {
            // accept "root-only" i.e. '/' object path
            if (1 == _name.length()) {
                return true;
            }
            separatorPos = _name.find(_separator, separatorPos+1);
        }
    }

    while (start != std::string::npos) {
        // DBus names parts must not be empty
        std::string part;

        if (isInitial) {
            isInitial = false;
        } else {
            start++;
        }

        if (separatorPos == std::string::npos) {
            part = _name.substr(start);
        } else {
            part = _name.substr(start, separatorPos-start);
        }

        if ("" == part) {
            COMMONAPI_ERROR(
                "Invalid interface name \'", _name,
                "\'. Must not contain empty parts.");
            return false;
        }

        // DBus name parts consist of the ASCII characters [0-9][A-Z][a-z]_,
        for (auto c : part) {
            // bus names may additionally contain [-]
            if (_isBusName && c == '-')
                continue;

            if (c < '0' ||
                (c > '9' && c < 'A') ||
                (c > 'Z' && c < '_') ||
                (c > '_' && c < 'a') ||
                c > 'z') {
                COMMONAPI_ERROR(
                    "Invalid interface name \'", _name,
                    "\'. Contains illegal character \'", c,
                    "\'. Only \'[0-9][A-Z][a-z]_\' are allowed.");
                return false;
            }
        }

        start = separatorPos;
        separatorPos = _name.find(_separator, separatorPos+1);
    }

    // DBus names must not exceed the maximum length
    if (_name.length() > DBUS_MAXIMUM_NAME_LENGTH) {
        COMMONAPI_ERROR(
            "Invalid interface name \'", _name,
            "\'. Size exceeds maximum size.");
        return false;
    }

    return true;
}

DBusType_t
DBusAddressTranslator::getDBusBusType(const ConnectionId_t &_connectionId) const {
    auto itsDbusTypesIterator = dbusTypes_.find(_connectionId);
    if(itsDbusTypesIterator != dbusTypes_.end()) {
        return itsDbusTypesIterator->second;
    } else {
        return DBusType_t::SESSION;
    }
}

bool DBusAddressTranslator::isOrgFreedesktopDBusPeerMapped() const {
    return orgFreedesktopDBusPeerMapped_;
}

bool DBusAddressTranslator::isValidVersion(const std::string& _version) const {
    if( _version == "" )
        return false;

    std::size_t itsSeparatorPos = _version.find('_');
    if (itsSeparatorPos == std::string::npos)
        return false;

    if( *(_version.begin()) != 'v')
        return false;

    for (auto it = _version.begin()+1; it != _version.end(); ++it) {
        if (!isdigit(*it) && *it != '_')
            return false;
    }
    return true;
}

} // namespace DBus
} // namespace CommonAPI
