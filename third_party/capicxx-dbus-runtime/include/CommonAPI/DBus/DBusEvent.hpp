// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUS_EVENT_HPP_
#define COMMONAPI_DBUS_DBUS_EVENT_HPP_

#include <CommonAPI/Event.hpp>
#include <CommonAPI/DBus/DBusAddress.hpp>
#include <CommonAPI/DBus/DBusHelper.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>
#include <CommonAPI/DBus/DBusProxyBase.hpp>
#include <CommonAPI/DBus/DBusProxyConnection.hpp>
#include <CommonAPI/DBus/DBusSerializableArguments.hpp>

namespace CommonAPI {
namespace DBus {

template <typename Event_, typename... Arguments_>
class DBusEvent: public Event_ {
public:
    typedef typename Event_::Subscription Subscription;
    typedef typename Event_::Listener Listener;

    DBusEvent(DBusProxyBase &_proxy,
              const std::string &_name, const std::string &_signature,
              std::tuple<Arguments_...> _arguments)
        : proxy_(_proxy),
          signalHandler_(std::make_shared<SignalHandler>(_proxy, this)),
          name_(_name), signature_(_signature),
          getMethodName_(""),
          arguments_(_arguments) {

        interface_ = proxy_.getDBusAddress().getInterface();
        path_ = proxy_.getDBusAddress().getObjectPath();
    }

    DBusEvent(DBusProxyBase &_proxy,
              const std::string &_name, const std::string &_signature,
              const std::string &_path, const std::string &_interface,
              std::tuple<Arguments_...> _arguments)
        : proxy_(_proxy),
          signalHandler_(std::make_shared<SignalHandler>(_proxy, this)),
          name_(_name), signature_(_signature),
          path_(_path), interface_(_interface),
          getMethodName_(""),
          arguments_(_arguments) {
    }

    DBusEvent(DBusProxyBase &_proxy,
              const std::string &_name,
              const std::string &_signature,
              const std::string &_getMethodName,
              std::tuple<Arguments_...> _arguments)
        : proxy_(_proxy),
          signalHandler_(std::make_shared<SignalHandler>(_proxy, this)),
          name_(_name),
          signature_(_signature),
          getMethodName_(_getMethodName),
          arguments_(_arguments) {

        interface_ = proxy_.getDBusAddress().getInterface();
        path_ = proxy_.getDBusAddress().getObjectPath();
    }

    virtual ~DBusEvent() {
        proxy_.removeSignalMemberHandler(subscription_, signalHandler_.get());
    }

    virtual void onError(const CommonAPI::CallStatus status) {
        (void)status;
    }

    virtual void onSpecificError(const CommonAPI::CallStatus status, const uint32_t tag) {
        (void)status;
        (void)tag;
    }

    virtual void setSubscriptionToken(const DBusProxyConnection::DBusSignalHandlerToken token, const uint32_t tag) {
        (void)token;
        (void)tag;
    }

 protected:

    class SignalHandler : public DBusProxyConnection::DBusSignalHandler,
                    public std::enable_shared_from_this<SignalHandler> {
    public:
        SignalHandler(DBusProxyBase&_proxy,
                DBusEvent<Event_, Arguments_ ...>* _dbusEvent) :
            proxy_(_proxy),
            dbusEvent_(_dbusEvent) {

        }

        virtual void onSignalDBusMessage(const DBusMessage &_message) {
            dbusEvent_->handleSignalDBusMessage(_message, typename make_sequence<sizeof...(Arguments_)>::type());
        }

        virtual void onInitialValueSignalDBusMessage(const DBusMessage&_message, const uint32_t tag) {
            dbusEvent_->handleSignalDBusMessage(tag, _message, typename make_sequence<sizeof...(Arguments_)>::type());
        }

        virtual void onSpecificError(const CommonAPI::CallStatus status, const uint32_t tag) {
            dbusEvent_->onSpecificError(status, tag);
        }

        virtual void setSubscriptionToken(const DBusProxyConnection::DBusSignalHandlerToken token, const uint32_t tag) {
            dbusEvent_->setSubscriptionToken(token, tag);
        }

    private :
        DBusProxyBase& proxy_;
        DBusEvent<Event_, Arguments_ ...>* dbusEvent_;
    };

    virtual void onFirstListenerAdded(const Listener &_listener) {
        (void)_listener;
        subscription_ = proxy_.addSignalMemberHandler(
                            path_, interface_, name_, signature_, getMethodName_, signalHandler_, false);
    }

    virtual void onListenerAdded(const Listener &_listener, const Subscription subscription) {
        (void)_listener;
        if ("" != getMethodName_) {
            proxy_.getCurrentValueForSignalListener(getMethodName_, signalHandler_, subscription);
        }
    }

    virtual void onLastListenerRemoved(const Listener&) {
        proxy_.removeSignalMemberHandler(subscription_, signalHandler_.get());
        std::get<0>(subscription_) = "";
        std::get<1>(subscription_) = "";
        std::get<2>(subscription_) = "";
        std::get<3>(subscription_) = "";
    }

    template<int ... Indices_>
    inline void handleSignalDBusMessage(const DBusMessage &_message, index_sequence<Indices_...>) {
        DBusInputStream input(_message);
        if (DBusSerializableArguments<
                Arguments_...
            >::deserialize(input, std::get<Indices_>(arguments_)...)) {
            this->notifyListeners(std::get<Indices_>(arguments_)...);
        }
    }

    template<int ... Indices_>
    inline void handleSignalDBusMessage(const uint32_t tag, const DBusMessage &_message, index_sequence<Indices_...>) {
        DBusInputStream input(_message);
        if (DBusSerializableArguments<
                Arguments_...
            >::deserialize(input, std::get<Indices_>(arguments_)...)) {
            this->notifySpecificListener(tag, std::get<Indices_>(arguments_)...);
        }
    }

    DBusProxyBase &proxy_;
    std::shared_ptr<SignalHandler> signalHandler_;

    std::string name_;
    std::string signature_;
    std::string path_;
    std::string interface_;
    std::string getMethodName_;

    DBusProxyConnection::DBusSignalHandlerToken subscription_;
    std::tuple<Arguments_...> arguments_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUS_EVENT_HPP_
