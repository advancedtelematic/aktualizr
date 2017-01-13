// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSSELECTIVEEVENT_HPP_
#define COMMONAPI_DBUS_DBUSSELECTIVEEVENT_HPP_

#include <CommonAPI/DBus/DBusEvent.hpp>

namespace CommonAPI {
namespace DBus {

template<typename EventType_, typename... Arguments_>
class DBusSelectiveEvent: public DBusEvent<EventType_, Arguments_...> {
public:
    typedef typename DBusEvent<EventType_, Arguments_...>::Listener Listener;
    typedef DBusEvent<EventType_, Arguments_...> DBusEventBase;

    DBusSelectiveEvent(DBusProxy &_proxy,
                       const char *_name, const char *_signature,
                       std::tuple<Arguments_...> _arguments)
        : DBusEventBase(_proxy, _name, _signature, _arguments) {
    }

    DBusSelectiveEvent(DBusProxy &_proxy,
                       const char *_name, const char *_signature,
                       const char *_path, const char *_interface,
                       std::tuple<Arguments_...> _arguments)
        : DBusEventBase(_proxy, _name, _signature, _path, _interface, _arguments) {
    }

    virtual ~DBusSelectiveEvent() {}

    virtual void onError(const CommonAPI::CallStatus status) {
        this->notifyError(status);
    }

protected:
    void onFirstListenerAdded(const Listener &) {
        bool success;
        this->subscription_
            = static_cast<DBusProxy&>(this->proxy_).subscribeForSelectiveBroadcastOnConnection(
                success, this->path_, this->interface_, this->name_, this->signature_, this);
                
        if (success == false) {
            // Call error listener with an error code
            this->notifyError(CommonAPI::CallStatus::SUBSCRIPTION_REFUSED);
        }
    }

    void onLastListenerRemoved(const Listener &) {
        static_cast<DBusProxy&>(this->proxy_).unsubscribeFromSelectiveBroadcast(
                        this->name_, this->subscription_, this);
    }
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSSELECTIVEEVENT_HPP_
