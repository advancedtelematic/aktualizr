// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSINSTANCEAVAILABILITYSTATUSCHANGED_EVENT_HPP_
#define COMMONAPI_DBUS_DBUSINSTANCEAVAILABILITYSTATUSCHANGED_EVENT_HPP_

#include <functional>
#include <future>
#include <string>
#include <vector>

#include <CommonAPI/ProxyManager.hpp>
#include <CommonAPI/DBus/DBusAddressTranslator.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>
#include <CommonAPI/DBus/DBusObjectManagerStub.hpp>
#include <CommonAPI/DBus/DBusInstanceAvailabilityStatusChangedEvent.hpp>
#include <CommonAPI/DBus/DBusTypes.hpp>

namespace CommonAPI {
namespace DBus {

// TODO Check to move logic to DBusServiceRegistry, now every proxy will deserialize the messages!
class DBusInstanceAvailabilityStatusChangedEvent:
                public ProxyManager::InstanceAvailabilityStatusChangedEvent,
                public DBusProxyConnection::DBusSignalHandler {
 public:
    DBusInstanceAvailabilityStatusChangedEvent(DBusProxy &_proxy, const std::string &_interfaceName) :
                    proxy_(_proxy),
                    observedInterfaceName_(_interfaceName) {
    }

    virtual ~DBusInstanceAvailabilityStatusChangedEvent() {
        proxy_.removeSignalMemberHandler(interfacesAddedSubscription_, this);
        proxy_.removeSignalMemberHandler(interfacesRemovedSubscription_, this);
    }

    virtual void onSignalDBusMessage(const DBusMessage& dbusMessage) {
        if (dbusMessage.hasMemberName("InterfacesAdded")) {
            onInterfacesAddedSignal(dbusMessage);
        } else if (dbusMessage.hasMemberName("InterfacesRemoved")) {
            onInterfacesRemovedSignal(dbusMessage);
        }
    }

 protected:
    virtual void onFirstListenerAdded(const Listener&) {
        interfacesAddedSubscription_ = proxy_.addSignalMemberHandler(
                        proxy_.getDBusAddress().getObjectPath(),
                        DBusObjectManagerStub::getInterfaceName(),
                        "InterfacesAdded",
                        "oa{sa{sv}}",
                        this,
                        false);

        interfacesRemovedSubscription_ = proxy_.addSignalMemberHandler(
                        proxy_.getDBusAddress().getObjectPath(),
                        DBusObjectManagerStub::getInterfaceName(),
                        "InterfacesRemoved",
                        "oas",
                        this,
                        false);
    }

    virtual void onLastListenerRemoved(const Listener&) {
        proxy_.removeSignalMemberHandler(interfacesAddedSubscription_, this);
        proxy_.removeSignalMemberHandler(interfacesRemovedSubscription_, this);
    }

 private:
    inline void onInterfacesAddedSignal(const DBusMessage &_message) {
        DBusInputStream dbusInputStream(_message);
        std::string dbusObjectPath;
        std::string dbusInterfaceName;
        DBusInterfacesAndPropertiesDict dbusInterfacesAndPropertiesDict;

        dbusInputStream >> dbusObjectPath;
        assert(!dbusInputStream.hasError());

        dbusInputStream.beginReadMapOfSerializableStructs();
        while (!dbusInputStream.readMapCompleted()) {
            dbusInputStream.align(8);
            dbusInputStream >> dbusInterfaceName;
            dbusInputStream.skipMap();
            assert(!dbusInputStream.hasError());
            if(dbusInterfaceName == observedInterfaceName_) {
                notifyInterfaceStatusChanged(dbusObjectPath, dbusInterfaceName, AvailabilityStatus::AVAILABLE);
            }
        }
        dbusInputStream.endReadMapOfSerializableStructs();
    }

    inline void onInterfacesRemovedSignal(const DBusMessage &_message) {
        DBusInputStream dbusInputStream(_message);
        std::string dbusObjectPath;
        std::vector<std::string> dbusInterfaceNames;

        dbusInputStream >> dbusObjectPath;
        assert(!dbusInputStream.hasError());

        dbusInputStream >> dbusInterfaceNames;
        assert(!dbusInputStream.hasError());

        for (const auto& dbusInterfaceName : dbusInterfaceNames) {
            if(dbusInterfaceName == observedInterfaceName_) {
                notifyInterfaceStatusChanged(dbusObjectPath, dbusInterfaceName, AvailabilityStatus::NOT_AVAILABLE);
            }
        }
    }

    void notifyInterfaceStatusChanged(const std::string &_objectPath,
                                      const std::string &_interfaceName,
                                      const AvailabilityStatus &_availability) {
        CommonAPI::Address itsAddress;
        DBusAddress itsDBusAddress(proxy_.getDBusAddress().getService(),
                                   _objectPath,
                                   _interfaceName);

        DBusAddressTranslator::get()->translate(itsDBusAddress, itsAddress);

        notifyListeners(itsAddress.getAddress(), _availability);
    }


    DBusProxy &proxy_;
    std::string observedInterfaceName_;
    DBusProxyConnection::DBusSignalHandlerToken interfacesAddedSubscription_;
    DBusProxyConnection::DBusSignalHandlerToken interfacesRemovedSubscription_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSINSTANCEAVAILABILITYSTATUSCHANGEDEVENT_HPP_
