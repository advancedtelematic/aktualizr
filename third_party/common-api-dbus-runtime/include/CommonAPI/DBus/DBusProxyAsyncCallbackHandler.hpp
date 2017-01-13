// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSPROXYASYNCCALLBACKHANDLER_HPP_
#define COMMONAPI_DBUS_DBUSPROXYASYNCCALLBACKHANDLER_HPP_

#include <functional>
#include <future>
#include <memory>

#include <CommonAPI/DBus/DBusHelper.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>
#include <CommonAPI/DBus/DBusProxyConnection.hpp>
#include <CommonAPI/DBus/DBusSerializableArguments.hpp>

namespace CommonAPI {
namespace DBus {

template<typename ... ArgTypes_>
class DBusProxyAsyncCallbackHandler:
        public DBusProxyConnection::DBusMessageReplyAsyncHandler {
 public:
    typedef std::function<void(CallStatus, ArgTypes_...)> FunctionType;

    static std::unique_ptr<DBusProxyConnection::DBusMessageReplyAsyncHandler> create(
            FunctionType&& callback, std::tuple<ArgTypes_...> args) {
        return std::unique_ptr<DBusProxyConnection::DBusMessageReplyAsyncHandler>(
                new DBusProxyAsyncCallbackHandler(std::move(callback), args));
    }

    DBusProxyAsyncCallbackHandler() = delete;
    DBusProxyAsyncCallbackHandler(FunctionType&& callback, std::tuple<ArgTypes_...> args)
        : callback_(std::move(callback)),
          args_(args),
          executionStarted_(false),
          executionFinished_(false),
          timeoutOccurred_(false),
          hasToBeDeleted_(false) {
    }
    virtual ~DBusProxyAsyncCallbackHandler() {}

    virtual std::future<CallStatus> getFuture() {
        return promise_.get_future();
    }

    virtual void onDBusMessageReply(const CallStatus& dbusMessageCallStatus,
            const DBusMessage& dbusMessage) {
        promise_.set_value(handleDBusMessageReply(dbusMessageCallStatus,
                dbusMessage,
                typename make_sequence<sizeof...(ArgTypes_)>::type(), args_));
    }

    virtual void setExecutionStarted() {
        executionStarted_ = true;
    }

    virtual bool getExecutionStarted() {
        return executionStarted_;
    }

    virtual void setExecutionFinished() {
        executionFinished_ = true;
        // free assigned std::function<> immediately
        callback_ = [](CallStatus, ArgTypes_...) {};
    }

    virtual bool getExecutionFinished() {
        return executionFinished_;
    }

    virtual void setTimeoutOccurred() {
        timeoutOccurred_ = true;
    }

    virtual bool getTimeoutOccurred() {
        return timeoutOccurred_;
    }

    virtual void setHasToBeDeleted() {
        hasToBeDeleted_ = true;
    }

    virtual bool hasToBeDeleted() {
        return hasToBeDeleted_;
    }

    virtual void lock() {
        asyncHandlerMutex_.lock();
    }

    virtual void unlock() {
        asyncHandlerMutex_.unlock();
    }

 private:
    template <int... ArgIndices_>
    inline CallStatus handleDBusMessageReply(
            const CallStatus dbusMessageCallStatus,
            const DBusMessage& dbusMessage,
            index_sequence<ArgIndices_...>,
            std::tuple<ArgTypes_...> argTuple) const {
        (void)argTuple; // this suppresses warning "set but not used" in case of empty _ArgTypes
                        // Looks like: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57560 - Bug 57650

        CallStatus callStatus = dbusMessageCallStatus;

        if (dbusMessageCallStatus == CallStatus::SUCCESS) {
            if (!dbusMessage.isErrorType()) {
                DBusInputStream dbusInputStream(dbusMessage);
                const bool success
                    = DBusSerializableArguments<ArgTypes_...>::deserialize(
                            dbusInputStream,
                            std::get<ArgIndices_>(argTuple)...);
                if (!success)
                    callStatus = CallStatus::REMOTE_ERROR;
            } else {
                callStatus = CallStatus::REMOTE_ERROR;
            }
        }

        callback_(callStatus, std::move(std::get<ArgIndices_>(argTuple))...);
        return callStatus;
    }

    std::promise<CallStatus> promise_;
    FunctionType callback_;
    std::tuple<ArgTypes_...> args_;
    bool executionStarted_;
    bool executionFinished_;
    bool timeoutOccurred_;
    bool hasToBeDeleted_;

    std::mutex asyncHandlerMutex_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSPROXYASYNCCALLBACKHANDLER_HPP_
