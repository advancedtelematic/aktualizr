// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSPROXYHELPER_HPP_
#define COMMONAPI_DBUS_DBUSPROXYHELPER_HPP_

#include <functional>
#include <future>
#include <memory>
#include <string>

#include <CommonAPI/DBus/DBusAddress.hpp>
#include <CommonAPI/DBus/DBusConfig.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>
#include <CommonAPI/DBus/DBusSerializableArguments.hpp>
#include <CommonAPI/DBus/DBusProxyAsyncCallbackHandler.hpp>
#include <CommonAPI/DBus/DBusProxyConnection.hpp>

namespace CommonAPI {
namespace DBus {

class DBusProxy;

template< class, class >
struct DBusProxyHelper;

#ifdef WIN32
// Visual Studio 2013 does not support 'magic statics' yet.
// Change back when switched to Visual Studio 2015 or higher.
static std::mutex callMethod_mutex_;
static std::mutex callMethodWithReply_mutex_;
static std::mutex callMethodAsync_mutex_;
#endif

template<
    template<class ...> class In_, class... InArgs_,
    template <class...> class Out_, class... OutArgs_>
struct DBusProxyHelper<In_<DBusInputStream, DBusOutputStream, InArgs_...>,
                           Out_<DBusInputStream, DBusOutputStream, OutArgs_...>> {

    template <typename DBusProxy_ = DBusProxy>
    static void callMethod(const DBusProxy_ &_proxy,
                           const std::string &_method,
                           const std::string &_signature,
                           const InArgs_&... _in,
                           CommonAPI::CallStatus &_status) {
#ifndef WIN32
        static std::mutex callMethod_mutex_;
#endif
        std::lock_guard<std::mutex> lock(callMethod_mutex_);

        if (_proxy.isAvailable()) {
            DBusMessage message = _proxy.createMethodCall(_method, _signature);
            if (sizeof...(InArgs_) > 0) {
                DBusOutputStream output(message);
                if (!DBusSerializableArguments<InArgs_...>::serialize(output, _in...)) {
                    _status = CallStatus::OUT_OF_MEMORY;
                    return;
                }
                output.flush();
            }

            const bool isSent = _proxy.getDBusConnection()->sendDBusMessage(message);
            _status = (isSent ? CallStatus::SUCCESS : CallStatus::OUT_OF_MEMORY);
        } else {
            _status = CallStatus::NOT_AVAILABLE;
        }
    }

    template <typename DBusProxy_ = DBusProxy>
    static void callMethodWithReply(
                    const DBusProxy_ &_proxy,
                    DBusMessage &_message,
                    const CommonAPI::CallInfo *_info,
                    const InArgs_&... _in,
                    CommonAPI::CallStatus &_status,
                    OutArgs_&... _out) {

        if (sizeof...(InArgs_) > 0) {
            DBusOutputStream output(_message);
            if (!DBusSerializableArguments<InArgs_...>::serialize(output, _in...)) {
                _status = CallStatus::OUT_OF_MEMORY;
                return;
            }
            output.flush();
        }

        DBusError error;
        DBusMessage reply = _proxy.getDBusConnection()->sendDBusMessageWithReplyAndBlock(_message, error, _info);
        if (error || !reply.isMethodReturnType()) {
            _status = CallStatus::REMOTE_ERROR;
            return;
        }

        if (sizeof...(OutArgs_) > 0) {
            DBusInputStream input(reply);
            if (!DBusSerializableArguments<OutArgs_...>::deserialize(input, _out...)) {
                _status = CallStatus::REMOTE_ERROR;
                return;
            }
        }
        _status = CallStatus::SUCCESS;
    }

    template <typename DBusProxy_ = DBusProxy>
    static void callMethodWithReply(
                const DBusProxy_ &_proxy,
                const DBusAddress &_address,
                const char *_method,
                const char *_signature,
                const CommonAPI::CallInfo *_info,
                const InArgs_&... _in,
                CommonAPI::CallStatus &_status,
                OutArgs_&... _out) {
#ifndef WIN32
        static std::mutex callMethodWithReply_mutex_;
#endif
        std::lock_guard<std::mutex> lock(callMethodWithReply_mutex_);

        if (_proxy.isAvailable()) {
            DBusMessage message = DBusMessage::createMethodCall(_address, _method, _signature);
            callMethodWithReply(_proxy, message, _info, _in..., _status, _out...);
        } else {
            _status = CallStatus::NOT_AVAILABLE;
        }
    }

    template <typename DBusProxy_ = DBusProxy>
    static void callMethodWithReply(
                const DBusProxy_ &_proxy,
                const std::string &_interface,
                const std::string &_method,
                const std::string &_signature,
                const CommonAPI::CallInfo *_info,
                const InArgs_&... _in,
                CommonAPI::CallStatus &_status,
                OutArgs_&... _out) {
        DBusAddress itsAddress(_proxy.getDBusAddress());
        itsAddress.setInterface(_interface);
        callMethodWithReply(
                _proxy, itsAddress,
                _method.c_str(), _signature.c_str(),
                _info,
                _in..., _status, _out...);
    }

    template <typename DBusProxy_ = DBusProxy>
    static void callMethodWithReply(
                    const DBusProxy_ &_proxy,
                    const std::string &_method,
                    const std::string &_signature,
                    const CommonAPI::CallInfo *_info,
                    const InArgs_&... _in,
                    CommonAPI::CallStatus &_status,
                    OutArgs_&... _out) {
#ifndef WIN32
        static std::mutex callMethodWithReply_mutex_;
#endif
        std::lock_guard<std::mutex> lock(callMethodWithReply_mutex_);

        if (_proxy.isAvailable()) {
            DBusMessage message = _proxy.createMethodCall(_method, _signature);
            callMethodWithReply(_proxy, message, _info, _in..., _status, _out...);
        } else {
            _status = CallStatus::NOT_AVAILABLE;
        }
    }

    template <typename DBusProxy_ = DBusProxy, typename AsyncCallback_>
    static std::future<CallStatus> callMethodAsync(
                    const DBusProxy_ &_proxy,
                    DBusMessage &_message,
                    const CommonAPI::CallInfo *_info,
                    const InArgs_&... _in,
                    AsyncCallback_ _callback,
                    std::tuple<OutArgs_...> _out) {
        if (sizeof...(InArgs_) > 0) {
            DBusOutputStream output(_message);
            const bool success = DBusSerializableArguments<
                                    InArgs_...
                                 >::serialize(output, _in...);
            if (!success) {
                std::promise<CallStatus> promise;
                promise.set_value(CallStatus::OUT_OF_MEMORY);
                return promise.get_future();
            }
            output.flush();
        }

        return _proxy.getDBusConnection()->sendDBusMessageWithReplyAsync(
                        _message,
                        DBusProxyAsyncCallbackHandler<
                            OutArgs_...
                        >::create(std::move(_callback), _out),
                        _info);
    }

    template <typename DBusProxy_ = DBusProxy, typename AsyncCallback_>
    static std::future<CallStatus> callMethodAsync(
                        const DBusProxy_ &_proxy,
                        const DBusAddress &_address,
                        const std::string &_method,
                        const std::string &_signature,
                        const CommonAPI::CallInfo *_info,
                        const InArgs_&... _in,
                        AsyncCallback_ _callback,
                        std::tuple<OutArgs_...> _out) {
#ifndef WIN32
        static std::mutex callMethodAsync_mutex_;
#endif
        std::lock_guard<std::mutex> lock(callMethodAsync_mutex_);

        if (_proxy.isAvailable()) {
            DBusMessage message = DBusMessage::createMethodCall(_address, _method, _signature);
            return callMethodAsync(_proxy, message, _info, _in..., _callback, _out);
        } else {
            CallStatus status = CallStatus::NOT_AVAILABLE;
            callCallbackOnNotAvailable(_callback, typename make_sequence<sizeof...(OutArgs_)>::type(), _out);

            std::promise<CallStatus> promise;
            promise.set_value(status);
            return promise.get_future();
        }
    }

    template <typename DBusProxy_ = DBusProxy, typename AsyncCallback_>
    static std::future<CallStatus> callMethodAsync(
                const DBusProxy_ &_proxy,
                const std::string &_interface,
                const std::string &_method,
                const std::string &_signature,
                const CommonAPI::CallInfo *_info,
                const InArgs_&... _in,
                AsyncCallback_ _callback,
                std::tuple<OutArgs_...> _out) {
        DBusAddress itsAddress(_proxy.getDBusAddress());
        itsAddress.setInterface(_interface);
        return callMethodAsync(
                    _proxy, itsAddress,
                    _method, _signature,
                    _info,
                    _in...,    _callback, _out);
    }

    template <typename DBusProxy_ = DBusProxy, typename AsyncCallback_>
    static std::future<CallStatus> callMethodAsync(
                    const DBusProxy_ &_proxy,
                    const std::string &_method,
                    const std::string &_signature,
                    const CommonAPI::CallInfo *_info,
                    const InArgs_&... _in,
                    AsyncCallback_ _callback,
                    std::tuple<OutArgs_...> _out) {
#ifndef WIN32
        static std::mutex callMethodAsync_mutex_;
#endif
        std::lock_guard<std::mutex> lock(callMethodAsync_mutex_);

        if (_proxy.isAvailable()) {
            DBusMessage message = _proxy.createMethodCall(_method, _signature);
            return callMethodAsync(_proxy, message, _info, _in..., _callback, _out);
        } else {
            callCallbackOnNotAvailable(
                _callback, typename make_sequence<sizeof...(OutArgs_)>::type(), _out);

            CallStatus status = CallStatus::NOT_AVAILABLE;
            std::promise<CallStatus> promise;
            promise.set_value(status);
            return promise.get_future();
        }
    }

    template <int... ArgIndices_>
    static void callCallbackOnNotAvailable(std::function<void(CallStatus, OutArgs_&...)> _callback,
                                           index_sequence<ArgIndices_...>, std::tuple<OutArgs_...> _out) {
        const CallStatus status(CallStatus::NOT_AVAILABLE);
        _callback(status, std::get<ArgIndices_>(_out)...);
        (void)_out;
    }
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSPROXYHELPER_HPP_
