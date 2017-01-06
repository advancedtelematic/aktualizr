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

    virtual void onSpecificError(const CommonAPI::CallStatus status, uint32_t tag) {
        this->notifySpecificError(tag, status);
    }

    virtual void setSubscriptionToken(const DBusProxyConnection::DBusSignalHandlerToken _subscriptionToken, uint32_t tag) {
        this->subscription_ = _subscriptionToken;
        static_cast<DBusProxy&>(this->proxy_).insertSelectiveSubscription(this->name_, this->signalHandler_, tag, this->signature_);
    }

protected:
    void onFirstListenerAdded(const Listener &) {

    }

    void onListenerAdded(const Listener &_listener, const uint32_t subscription) {
        (void) _listener;
        static_cast<DBusProxy&>(this->proxy_).subscribeForSelectiveBroadcastOnConnection(
                this->path_, this->interface_, this->name_, this->signature_, this->signalHandler_, subscription);
    }

    void onLastListenerRemoved(const Listener &) {
        static_cast<DBusProxy&>(this->proxy_).unsubscribeFromSelectiveBroadcast(
                        this->name_, this->subscription_, this->signalHandler_.get());
    }
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSSELECTIVEEVENT_HPP_
