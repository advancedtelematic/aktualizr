// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSPROXYASYNCSIGNALMEMBERCALLBACKHANDLER_HPP_
#define COMMONAPI_DBUS_DBUSPROXYASYNCSIGNALMEMBERCALLBACKHANDLER_HPP_

#include <functional>
#include <future>
#include <memory>

//#include <CommonAPI/DBus/DBusHelper.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>
#include <CommonAPI/DBus/DBusProxyConnection.hpp>

namespace CommonAPI {
namespace DBus {

template <typename DelegateObjectType_>
class DBusProxyAsyncSignalMemberCallbackHandler: public DBusProxyConnection::DBusMessageReplyAsyncHandler {
 public:

    struct Delegate {
        typedef std::function<void(CallStatus, DBusMessage, std::weak_ptr<DBusProxyConnection::DBusSignalHandler>, uint32_t)> FunctionType;

        Delegate(std::shared_ptr<DelegateObjectType_> object, FunctionType function) :
            function_(std::move(function)) {
            object_ = object;
        }
        std::weak_ptr<DelegateObjectType_> object_;
        FunctionType function_;
    };

    static std::unique_ptr<DBusProxyConnection::DBusMessageReplyAsyncHandler> create(
            Delegate& delegate, std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
            const uint32_t tag) {
        return std::unique_ptr<DBusProxyConnection::DBusMessageReplyAsyncHandler>(
                new DBusProxyAsyncSignalMemberCallbackHandler<DelegateObjectType_>(std::move(delegate), dbusSignalHandler, tag));
    }

    DBusProxyAsyncSignalMemberCallbackHandler() = delete;
    DBusProxyAsyncSignalMemberCallbackHandler(Delegate&& delegate,
                                              std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
                                              const uint32_t tag):
        delegate_(std::move(delegate)), dbusSignalHandler_(dbusSignalHandler), tag_(tag),
        executionStarted_(false), executionFinished_(false),
        timeoutOccurred_(false), hasToBeDeleted_(false) {
    }
    virtual ~DBusProxyAsyncSignalMemberCallbackHandler() {}

    virtual std::future<CallStatus> getFuture() {
        return promise_.get_future();
    }

    virtual void onDBusMessageReply(const CallStatus& dbusMessageCallStatus, const DBusMessage& dbusMessage) {
        promise_.set_value(handleDBusMessageReply(dbusMessageCallStatus, dbusMessage));
    }

    virtual void setExecutionStarted() {
        executionStarted_ = true;
    }

    virtual bool getExecutionStarted() {
        return executionStarted_;
    }

    virtual void setExecutionFinished() {
        executionFinished_ = true;
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
    inline CallStatus handleDBusMessageReply(const CallStatus dbusMessageCallStatus, const DBusMessage& dbusMessage) const {
        CallStatus callStatus = dbusMessageCallStatus;

        //check if object is expired
        if(!delegate_.object_.expired())
            delegate_.function_(callStatus, dbusMessage, dbusSignalHandler_, tag_);

        return callStatus;
    }

    std::promise<CallStatus> promise_;
    const Delegate delegate_;
    std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler_;
    const uint32_t tag_;
    bool executionStarted_;
    bool executionFinished_;
    bool timeoutOccurred_;
    bool hasToBeDeleted_;

    std::mutex asyncHandlerMutex_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSPROXYASYNCSIGNALMEMBERCALLBACKHANDLER_HPP_
