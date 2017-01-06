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
#include <CommonAPI/DBus/DBusErrorEvent.hpp>

namespace CommonAPI {
namespace DBus {

template <class, class...>
class DBusProxyAsyncCallbackHandler;

template <class DelegateObjectType_, class ... ArgTypes_>
class DBusProxyAsyncCallbackHandler :
        public DBusProxyConnection::DBusMessageReplyAsyncHandler {
 public:

    struct Delegate {
        typedef std::function<void(CallStatus, ArgTypes_...)> FunctionType;

        Delegate(std::shared_ptr<DelegateObjectType_> _object, FunctionType _function) :
            function_(std::move(_function)) {
            object_ = _object;
        }
        std::weak_ptr<DelegateObjectType_> object_;
        FunctionType function_;
    };

    static std::unique_ptr<DBusProxyConnection::DBusMessageReplyAsyncHandler> create(
            Delegate& _delegate, const std::tuple<ArgTypes_...>& _args) {
        return std::unique_ptr<DBusProxyConnection::DBusMessageReplyAsyncHandler>(
                new DBusProxyAsyncCallbackHandler<DelegateObjectType_, ArgTypes_...>(std::move(_delegate), _args));
    }

    DBusProxyAsyncCallbackHandler() = delete;
    DBusProxyAsyncCallbackHandler(Delegate&& _delegate, const std::tuple<ArgTypes_...>& _args)
        : delegate_(std::move(_delegate)),
          args_(_args),
          executionStarted_(false),
          executionFinished_(false),
          timeoutOccurred_(false),
          hasToBeDeleted_(false) {
    }
    virtual ~DBusProxyAsyncCallbackHandler() {
        // free assigned std::function<> immediately
        delegate_.function_ = [](CallStatus, ArgTypes_...) {};
    }

    virtual std::future<CallStatus> getFuture() {
        return promise_.get_future();
    }

    virtual void onDBusMessageReply(const CallStatus& _dbusMessageCallStatus,
            const DBusMessage& _dbusMessage) {
        promise_.set_value(handleDBusMessageReply(_dbusMessageCallStatus,
                _dbusMessage,
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

 protected:
    Delegate delegate_;
    std::promise<CallStatus> promise_;
    std::tuple<ArgTypes_...> args_;

 private:
    template <int... ArgIndices_>
    inline CallStatus handleDBusMessageReply(
            const CallStatus _dbusMessageCallStatus,
            const DBusMessage& _dbusMessage,
            index_sequence<ArgIndices_...>,
            std::tuple<ArgTypes_...>& _argTuple) const {
        (void)_argTuple; // this suppresses warning "set but not used" in case of empty _ArgTypes
                        // Looks like: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57560 - Bug 57650

        CallStatus callStatus = _dbusMessageCallStatus;

        if (_dbusMessageCallStatus == CallStatus::SUCCESS) {
            DBusInputStream dbusInputStream(_dbusMessage);
            if(DBusSerializableArguments<ArgTypes_...>::deserialize(dbusInputStream,
                    std::get<ArgIndices_>(_argTuple)...)) {
            } else {
                callStatus = CallStatus::REMOTE_ERROR;
            }
        }

        //ensure that delegate object (e.g. Proxy) survives
        if(auto itsDelegateObject = delegate_.object_.lock())
            delegate_.function_(callStatus, std::move(std::get<ArgIndices_>(_argTuple))...);

        return callStatus;
    }

    bool executionStarted_;
    bool executionFinished_;
    bool timeoutOccurred_;
    bool hasToBeDeleted_;

    std::mutex asyncHandlerMutex_;
};

template<
    template <class...> class ErrorEventsTuple_, class... ErrorEvents_,
    class DelegateObjectType_, class... ArgTypes_>
class DBusProxyAsyncCallbackHandler<
    ErrorEventsTuple_<ErrorEvents_...>,
    DelegateObjectType_,
    ArgTypes_...>:
        public DBusProxyAsyncCallbackHandler<DelegateObjectType_, ArgTypes_...> {

public:

    static std::unique_ptr<DBusProxyConnection::DBusMessageReplyAsyncHandler> create(
            typename DBusProxyAsyncCallbackHandler<DelegateObjectType_, ArgTypes_...>::Delegate& _delegate,
            const std::tuple<ArgTypes_...>& _args,
            const ErrorEventsTuple_<ErrorEvents_...> &_errorEvents) {
        return std::unique_ptr<DBusProxyConnection::DBusMessageReplyAsyncHandler>(
                new DBusProxyAsyncCallbackHandler<ErrorEventsTuple_<ErrorEvents_...>, DelegateObjectType_, ArgTypes_...>(std::move(_delegate), _args, _errorEvents));
    }

    DBusProxyAsyncCallbackHandler() = delete;
    DBusProxyAsyncCallbackHandler(typename DBusProxyAsyncCallbackHandler<DelegateObjectType_, ArgTypes_...>::Delegate&& _delegate,
                                  const std::tuple<ArgTypes_...>& _args,
                                  const ErrorEventsTuple_<ErrorEvents_...> &_errorEvents) :
                                      DBusProxyAsyncCallbackHandler<DelegateObjectType_, ArgTypes_...>(std::move(_delegate), _args),
                                      errorEvents_(_errorEvents) {}

    virtual ~DBusProxyAsyncCallbackHandler() {
        // free assigned std::function<> immediately
        this->delegate_.function_ = [](CallStatus, ArgTypes_...) {};
    }

    virtual void onDBusMessageReply(const CallStatus& _dbusMessageCallStatus,
            const DBusMessage& _dbusMessage) {
        this->promise_.set_value(handleDBusMessageReply(
                _dbusMessageCallStatus,
                _dbusMessage,
                typename make_sequence<sizeof...(ArgTypes_)>::type(), this->args_));
    }

private:

    template <int... ArgIndices_>
    inline CallStatus handleDBusMessageReply(
            const CallStatus _dbusMessageCallStatus,
            const DBusMessage& _dbusMessage,
            index_sequence<ArgIndices_...>,
            std::tuple<ArgTypes_...>& _argTuple) const {
        (void)_argTuple; // this suppresses warning "set but not used" in case of empty _ArgTypes
                        // Looks like: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57560 - Bug 57650

        CallStatus callStatus = _dbusMessageCallStatus;

        if (_dbusMessageCallStatus == CallStatus::SUCCESS) {
            DBusInputStream dbusInputStream(_dbusMessage);
            if(DBusSerializableArguments<ArgTypes_...>::deserialize(dbusInputStream,
                    std::get<ArgIndices_>(_argTuple)...)) {
            } else {
                callStatus = CallStatus::REMOTE_ERROR;
            }
        } else {
            if(_dbusMessage.isErrorType()) {
                //ensure that delegate object (e.g. Proxy and its error events) survives
                if(auto itsDelegateObject = this->delegate_.object_.lock()) {
                    DBusErrorEventHelper::notifyListeners(_dbusMessage,
                                                          _dbusMessage.getError(),
                                                          typename make_sequence_range<sizeof...(ErrorEvents_), 0>::type(),
                                                          errorEvents_);
                }
            }
        }

        //ensure that delegate object (e.g. Proxy) survives
        if(auto itsDelegateObject = this->delegate_.object_.lock())
            this->delegate_.function_(callStatus, std::move(std::get<ArgIndices_>(_argTuple))...);

        return callStatus;
    }

    ErrorEventsTuple_<ErrorEvents_...> errorEvents_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSPROXYASYNCCALLBACKHANDLER_HPP_
