// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sstream>

#include <CommonAPI/Utils.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>
#include <CommonAPI/DBus/DBusUtils.hpp>
#include <CommonAPI/DBus/DBusProxyAsyncSignalMemberCallbackHandler.hpp>
#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/Logger.hpp>

namespace CommonAPI {
namespace DBus {

DBusProxyStatusEvent::DBusProxyStatusEvent(DBusProxy *_dbusProxy)
    : dbusProxy_(_dbusProxy) {
}

void DBusProxyStatusEvent::onListenerAdded(const Listener &_listener,
                                           const Subscription _subscription) {
    std::lock_guard<std::recursive_mutex> listenersLock(listenersMutex_);

    //notify listener about availability status -> push function to mainloop
    std::weak_ptr<DBusProxy> itsdbusProxy = dbusProxy_->shared_from_this();
    dbusProxy_->getDBusConnection()->proxyPushFunctionToMainLoop<DBusConnection>(
            DBusProxy::notifySpecificListener,
            itsdbusProxy,
            _listener,
            _subscription);
}

void DBusProxyStatusEvent::onListenerRemoved(const Listener& _listener,
                                             const Subscription _subscription) {
    std::lock_guard<std::recursive_mutex> listenersLock(listenersMutex_);
    (void)_listener;
    auto listenerIt = listeners_.begin();
    while(listenerIt != listeners_.end()) {
        if(listenerIt->first == _subscription)
            listenerIt = listeners_.erase(listenerIt);
        else
            ++listenerIt;
    }
}

void DBusProxy::availabilityTimeoutThreadHandler() const {
    std::unique_lock<std::mutex> threadLock(availabilityTimeoutThreadMutex_);

    bool cancel = false;
    bool firstIteration = true;

    // the callbacks that have to be done are stored with
    // their required data in a list of tuples.
    typedef std::tuple<
            isAvailableAsyncCallback,
            std::promise<AvailabilityStatus>,
            AvailabilityStatus,
            std::chrono::steady_clock::time_point
            > CallbackData_t;
    std::list<CallbackData_t> callbacks;

    while(!cancel) {

        //get min timeout
        timeoutsMutex_.lock();

        int timeout = std::numeric_limits<int>::max();
        std::chrono::steady_clock::time_point minTimeout;
        if (timeouts_.size() > 0) {
            auto minTimeoutElement = std::min_element(timeouts_.begin(), timeouts_.end(),
                    [] (const AvailabilityTimeout_t& lhs, const AvailabilityTimeout_t& rhs) {
                        return std::get<0>(lhs) < std::get<0>(rhs);
            });
            minTimeout = std::get<0>(*minTimeoutElement);
            std::chrono::steady_clock::time_point now = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();
            timeout = (int)std::chrono::duration_cast<std::chrono::milliseconds>(minTimeout - now).count();
        }
        timeoutsMutex_.unlock();

        //wait for timeout or notification
        if (!firstIteration && std::cv_status::timeout ==
                    availabilityTimeoutCondition_.wait_for(threadLock, std::chrono::milliseconds(timeout))) {
            timeoutsMutex_.lock();

            //iterate through timeouts
            auto it = timeouts_.begin();
            while (it != timeouts_.end()) {
                std::chrono::steady_clock::time_point now = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();

                isAvailableAsyncCallback callback = std::get<1>(*it);

                if (now > std::get<0>(*it)) {
                    //timeout
                    availabilityMutex_.lock();
                    std::chrono::steady_clock::time_point timepoint_;
                    if(isAvailable())
                        callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                            AvailabilityStatus::AVAILABLE,
                                                            timepoint_));
                    else
                        callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                            AvailabilityStatus::NOT_AVAILABLE,
                                                            timepoint_));
                    it = timeouts_.erase(it);
                    availabilityMutex_.unlock();
                } else {
                    //timeout not expired
                    availabilityMutex_.lock();
                    if(isAvailable()) {
                        callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                            AvailabilityStatus::AVAILABLE,
                                                            minTimeout));
                        it = timeouts_.erase(it);
                    } else {
                        ++it;
                    }
                    availabilityMutex_.unlock();
                }
            }

            timeoutsMutex_.unlock();
        } else {

            if(firstIteration) {
                firstIteration = false;
                continue;
            }

            //timeout not expired
            timeoutsMutex_.lock();
            auto it = timeouts_.begin();
            while (it != timeouts_.end()) {
                isAvailableAsyncCallback callback = std::get<1>(*it);

                availabilityMutex_.lock();
                if(isAvailable()) {
                    callbacks.push_back(std::make_tuple(callback, std::move(std::get<2>(*it)),
                                                        AvailabilityStatus::AVAILABLE,
                                                        minTimeout));
                    it = timeouts_.erase(it);
                } else {
                    ++it;
                }
                availabilityMutex_.unlock();
            }

            timeoutsMutex_.unlock();
        }

        //do callbacks
        isAvailableAsyncCallback callback;
        AvailabilityStatus avStatus;
        int remainingTimeout;
        std::chrono::steady_clock::time_point now;

        auto it = callbacks.begin();
        while(it != callbacks.end()) {
            callback = std::get<0>(*it);
            avStatus = std::get<2>(*it);

            // compute remaining timeout
            now = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();
            remainingTimeout = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::get<3>(*it) - now).count();
            if(remainingTimeout < 0)
                remainingTimeout = 0;

            threadLock.unlock();

            std::get<1>(*it).set_value(avStatus);
            callback(avStatus, remainingTimeout);

            threadLock.lock();

            it = callbacks.erase(it);
        }

        //cancel thread
        timeoutsMutex_.lock();
        if(timeouts_.size() == 0 && callbacks.size() == 0)
            cancel = true;
        timeoutsMutex_.unlock();
    }
}

DBusProxy::DBusProxy(const DBusAddress &_dbusAddress,
                     const std::shared_ptr<DBusProxyConnection> &_connection):
                DBusProxyBase(_dbusAddress, _connection),
                dbusProxyStatusEvent_(this),
                availabilityStatus_(AvailabilityStatus::UNKNOWN),
                interfaceVersionAttribute_(*this, "uu", "getInterfaceVersion"),
                dbusServiceRegistry_(DBusServiceRegistry::get(_connection))
{
}

void DBusProxy::init() {
    std::weak_ptr<DBusProxy> itsProxy = shared_from_this();
    dbusServiceRegistrySubscription_ = dbusServiceRegistry_->subscribeAvailabilityListener(
                    getAddress().getAddress(),
                    std::bind(&DBusProxy::onDBusServiceInstanceStatus, this, std::placeholders::_1, std::placeholders::_2),
                    itsProxy);
}

DBusProxy::~DBusProxy() {
    if(availabilityTimeoutThread_) {
        if(availabilityTimeoutThread_->joinable())
            availabilityTimeoutThread_->join();
    }
    dbusServiceRegistry_->unsubscribeAvailabilityListener(
                    getAddress().getAddress(),
                    dbusServiceRegistrySubscription_);
    Factory::get()->decrementConnection(connection_);
}

bool DBusProxy::isAvailable() const {
    return (availabilityStatus_ == AvailabilityStatus::AVAILABLE);
}

bool DBusProxy::isAvailableBlocking() const {
    std::unique_lock<std::mutex> lock(availabilityMutex_);

    if(!getDBusConnection()->hasDispatchThread()) {
        return isAvailable();
    }

    while (availabilityStatus_ != AvailabilityStatus::AVAILABLE) {
        availabilityCondition_.wait(lock);
    }

    return true;
}

std::future<AvailabilityStatus> DBusProxy::isAvailableAsync(
            isAvailableAsyncCallback _callback,
            const CommonAPI::CallInfo *_info) const {

    std::promise<AvailabilityStatus> promise;
    std::future<AvailabilityStatus> future = promise.get_future();

    //set timeout point
    auto timeoutPoint = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now() + std::chrono::milliseconds(_info->timeout_);

    timeoutsMutex_.lock();
    if(timeouts_.size() == 0) {
        //no timeouts

        bool isAvailabilityTimeoutThread = false;

        //join running availability thread
        if(availabilityTimeoutThread_) {

            //check if current thread is availability timeout thread
            isAvailabilityTimeoutThread = (std::this_thread::get_id() ==
                            availabilityTimeoutThread_.get()->get_id());

            if(availabilityTimeoutThread_->joinable() && !isAvailabilityTimeoutThread) {
                timeoutsMutex_.unlock();
                availabilityTimeoutThread_->join();
                timeoutsMutex_.lock();
            }
        }
        //add new timeout
        timeouts_.push_back(std::make_tuple(timeoutPoint, _callback, std::move(promise)));

        //start availability thread
        if(!isAvailabilityTimeoutThread)
            availabilityTimeoutThread_ = std::make_shared<std::thread>(
                    std::bind(&DBusProxy::availabilityTimeoutThreadHandler, this));
    } else {
        //add timeout
        timeouts_.push_back(std::make_tuple(timeoutPoint, _callback, std::move(promise)));
    }
    timeoutsMutex_.unlock();

    availabilityTimeoutThreadMutex_.lock();
    //notify availability thread that new timeout was added
    availabilityTimeoutCondition_.notify_all();
    availabilityTimeoutThreadMutex_.unlock();

    return future;
}

AvailabilityStatus DBusProxy::getAvailabilityStatus() const {
    return availabilityStatus_;
}

ProxyStatusEvent& DBusProxy::getProxyStatusEvent() {
    return dbusProxyStatusEvent_;
}

InterfaceVersionAttribute& DBusProxy::getInterfaceVersionAttribute() {
    return interfaceVersionAttribute_;
}

void DBusProxy::signalMemberCallback(const CallStatus _status,
        const DBusMessage& dbusMessage,
        std::weak_ptr<DBusProxyConnection::DBusSignalHandler> _handler,
        const uint32_t _tag) {
    (void)_status;
    (void)_tag;
    if(auto itsHandler = _handler.lock())
        itsHandler->onSignalDBusMessage(dbusMessage);
}

void DBusProxy::signalInitialValueCallback(const CallStatus _status,
        const DBusMessage &_message,
        std::weak_ptr<DBusProxyConnection::DBusSignalHandler> _handler,
        const uint32_t _tag) {
    if (_status != CallStatus::SUCCESS) {
        COMMONAPI_ERROR("Error when receiving initial value of an attribute");
    } else {
        if(auto itsHandler = _handler.lock())
            itsHandler->onInitialValueSignalDBusMessage(_message, _tag);
    }
}

void DBusProxy::notifySpecificListener(std::weak_ptr<DBusProxy> _dbusProxy,
                                       const ProxyStatusEvent::Listener &_listener,
                                       const ProxyStatusEvent::Subscription _subscription) {
    if(auto itsDbusProxy = _dbusProxy.lock()) {
        std::lock_guard<std::recursive_mutex> listenersLock(itsDbusProxy->dbusProxyStatusEvent_.listenersMutex_);

        AvailabilityStatus itsStatus = itsDbusProxy->availabilityStatus_;
        if (itsStatus != AvailabilityStatus::UNKNOWN)
            itsDbusProxy->dbusProxyStatusEvent_.notifySpecificListener(_subscription, itsStatus);

        //add listener to list so that it can be notified about a change of availability
        itsDbusProxy->dbusProxyStatusEvent_.listeners_.push_back(std::make_pair(_subscription, _listener));
    }
}

void DBusProxy::onDBusServiceInstanceStatus(std::shared_ptr<DBusProxy> _proxy,
                                            const AvailabilityStatus& availabilityStatus) {
    (void)_proxy;
    if (availabilityStatus != availabilityStatus_) {
        availabilityMutex_.lock();
        availabilityStatus_ = availabilityStatus;
        availabilityMutex_.unlock();

        availabilityTimeoutThreadMutex_.lock();
        //notify availability thread that proxy status has changed
        availabilityTimeoutCondition_.notify_all();
        availabilityTimeoutThreadMutex_.unlock();

        {
            std::lock_guard<std::recursive_mutex> subscribersLock(dbusProxyStatusEvent_.listenersMutex_);
            for(auto listenerIt : dbusProxyStatusEvent_.listeners_)
                dbusProxyStatusEvent_.notifySpecificListener(listenerIt.first, availabilityStatus_);
        }

        if (availabilityStatus == AvailabilityStatus::AVAILABLE) {
            std::lock_guard < std::mutex > queueLock(signalMemberHandlerQueueMutex_);

            for(auto signalMemberHandlerIterator = signalMemberHandlerQueue_.begin();
                    signalMemberHandlerIterator != signalMemberHandlerQueue_.end();
                    signalMemberHandlerIterator++) {

                if (!std::get<7>(*signalMemberHandlerIterator)) {
                    connection_->addSignalMemberHandler(
                                std::get<0>(*signalMemberHandlerIterator),
                                std::get<1>(*signalMemberHandlerIterator),
                                std::get<2>(*signalMemberHandlerIterator),
                                std::get<3>(*signalMemberHandlerIterator),
                                std::get<5>(*signalMemberHandlerIterator),
                                std::get<6>(*signalMemberHandlerIterator));
                    std::get<7>(*signalMemberHandlerIterator) = true;

                    DBusMessage message = createMethodCall(std::get<4>(*signalMemberHandlerIterator), "");

                    DBusProxyAsyncSignalMemberCallbackHandler<DBusProxy>::Delegate::FunctionType myFunc = std::bind(
                            &DBusProxy::signalMemberCallback,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2,
                            std::placeholders::_3,
                            std::placeholders::_4);
                    DBusProxyAsyncSignalMemberCallbackHandler<DBusProxy>::Delegate delegate(shared_from_this(), myFunc);
                    connection_->sendDBusMessageWithReplyAsync(
                            message,
                            DBusProxyAsyncSignalMemberCallbackHandler<DBusProxy>::create(delegate, std::get<5>(*signalMemberHandlerIterator), 0),
                            &defaultCallInfo);
                }
            }
            {
                std::lock_guard < std::mutex > queueLock(selectiveBroadcastHandlersMutex_);
                for (auto selectiveBroadcasts : selectiveBroadcastHandlers) {
                    connection_->sendPendingSelectiveSubscription(this,
                            selectiveBroadcasts.first,
                            std::get<0>(selectiveBroadcasts.second),
                            std::get<1>(selectiveBroadcasts.second),
                            std::get<2>(selectiveBroadcasts.second));
                }
            }
        } else {
            std::lock_guard < std::mutex > queueLock(signalMemberHandlerQueueMutex_);

            for(auto signalMemberHandlerIterator = signalMemberHandlerQueue_.begin();
                    signalMemberHandlerIterator != signalMemberHandlerQueue_.end();
                    signalMemberHandlerIterator++) {

                auto itsHandler = std::get<5>(*signalMemberHandlerIterator).lock();
                if (itsHandler && std::get<7>(*signalMemberHandlerIterator)) {
                    DBusProxyConnection::DBusSignalHandlerToken signalHandlerToken (
                            std::get<0>(*signalMemberHandlerIterator),
                            std::get<1>(*signalMemberHandlerIterator),
                            std::get<2>(*signalMemberHandlerIterator),
                            std::get<3>(*signalMemberHandlerIterator));
                    connection_->removeSignalMemberHandler(signalHandlerToken, itsHandler.get());
                    std::get<7>(*signalMemberHandlerIterator) = false;
                }
            }
        }
    }
    availabilityMutex_.lock();
    availabilityCondition_.notify_one();
    availabilityMutex_.unlock();
}

void DBusProxy::insertSelectiveSubscription(const std::string& interfaceMemberName,
            std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
            uint32_t tag,
            std::string interfaceMemberSignature) {
    std::lock_guard < std::mutex > queueLock(selectiveBroadcastHandlersMutex_);
    selectiveBroadcastHandlers[interfaceMemberName] = std::make_tuple(
            dbusSignalHandler, tag, interfaceMemberSignature);
}

void DBusProxy::subscribeForSelectiveBroadcastOnConnection(
                                                      const std::string& objectPath,
                                                      const std::string& interfaceName,
                                                      const std::string& interfaceMemberName,
                                                      const std::string& interfaceMemberSignature,
                                                      std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
                                                      uint32_t tag) {

    getDBusConnection()->subscribeForSelectiveBroadcast(
        objectPath,
        interfaceName,
        interfaceMemberName,
        interfaceMemberSignature,
        dbusSignalHandler,
        this,
        tag);
}

void DBusProxy::unsubscribeFromSelectiveBroadcast(const std::string& eventName,
                                                 DBusProxyConnection::DBusSignalHandlerToken subscription,
                                                 const DBusProxyConnection::DBusSignalHandler* dbusSignalHandler) {

    getDBusConnection()->unsubscribeFromSelectiveBroadcast(eventName, subscription, this, dbusSignalHandler);

    std::lock_guard < std::mutex > queueLock(selectiveBroadcastHandlersMutex_);
    std::string interfaceMemberName = std::get<2>(subscription);
    auto its_handler = selectiveBroadcastHandlers.find(interfaceMemberName);
    if (its_handler != selectiveBroadcastHandlers.end()) {
        selectiveBroadcastHandlers.erase(interfaceMemberName);
    }
}

DBusProxyConnection::DBusSignalHandlerToken DBusProxy::addSignalMemberHandler(
                const std::string& objectPath,
                const std::string& interfaceName,
                const std::string& signalName,
                const std::string& signalSignature,
                std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
                const bool justAddFilter) {
    return DBusProxyBase::addSignalMemberHandler(
                objectPath,
                interfaceName,
                signalName,
                signalSignature,
                dbusSignalHandler,
                justAddFilter);
}

DBusProxyConnection::DBusSignalHandlerToken DBusProxy::addSignalMemberHandler(
        const std::string &objectPath,
        const std::string &interfaceName,
        const std::string &signalName,
        const std::string &signalSignature,
        const std::string &getMethodName,
        std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
        const bool justAddFilter) {

    DBusProxyConnection::DBusSignalHandlerToken signalHandlerToken (
            objectPath,
            interfaceName,
            signalName,
            signalSignature);

    if (getMethodName != "") {

        SignalMemberHandlerTuple signalMemberHandler(
            objectPath,
            interfaceName,
            signalName,
            signalSignature,
            getMethodName,
            dbusSignalHandler,
            justAddFilter,
            false);

        availabilityMutex_.lock();
        if (availabilityStatus_ == AvailabilityStatus::AVAILABLE) {
            availabilityMutex_.unlock();
            signalHandlerToken = connection_->addSignalMemberHandler(
                objectPath,
                interfaceName,
                signalName,
                signalSignature,
                dbusSignalHandler,
                justAddFilter);
            std::get<7>(signalMemberHandler) = true;
        } else {
            availabilityMutex_.unlock();
        }
        addSignalMemberHandlerToQueue(signalMemberHandler);
    } else {
        signalHandlerToken = connection_->addSignalMemberHandler(
                objectPath,
                interfaceName,
                signalName,
                signalSignature,
                dbusSignalHandler,
                justAddFilter);
    }

    return signalHandlerToken;
}

void DBusProxy::addSignalMemberHandlerToQueue(SignalMemberHandlerTuple& _signalMemberHandler) {

    std::lock_guard < std::mutex > queueLock(signalMemberHandlerQueueMutex_);
    bool found = false;

    for(auto signalMemberHandlerIterator = signalMemberHandlerQueue_.begin();
            signalMemberHandlerIterator != signalMemberHandlerQueue_.end();
            signalMemberHandlerIterator++) {

        if ( (std::get<0>(*signalMemberHandlerIterator) == std::get<0>(_signalMemberHandler)) &&
                (std::get<1>(*signalMemberHandlerIterator) == std::get<1>(_signalMemberHandler)) &&
                (std::get<2>(*signalMemberHandlerIterator) == std::get<2>(_signalMemberHandler)) &&
                (std::get<3>(*signalMemberHandlerIterator) == std::get<3>(_signalMemberHandler))) {

            found = true;
            break;
        }
    }
    if (!found) {
        signalMemberHandlerQueue_.push_back(_signalMemberHandler);
    }
}

bool DBusProxy::removeSignalMemberHandler(
            const DBusProxyConnection::DBusSignalHandlerToken &_dbusSignalHandlerToken,
            const DBusProxyConnection::DBusSignalHandler* _dbusSignalHandler) {

    {
        std::lock_guard < std::mutex > queueLock(signalMemberHandlerQueueMutex_);
        for(auto signalMemberHandlerIterator = signalMemberHandlerQueue_.begin();
                signalMemberHandlerIterator != signalMemberHandlerQueue_.end();
                signalMemberHandlerIterator++) {

            if ( (std::get<0>(*signalMemberHandlerIterator) == std::get<0>(_dbusSignalHandlerToken)) &&
                    (std::get<1>(*signalMemberHandlerIterator) == std::get<1>(_dbusSignalHandlerToken)) &&
                    (std::get<2>(*signalMemberHandlerIterator) == std::get<2>(_dbusSignalHandlerToken)) &&
                    (std::get<3>(*signalMemberHandlerIterator) == std::get<3>(_dbusSignalHandlerToken))) {
                signalMemberHandlerIterator = signalMemberHandlerQueue_.erase(signalMemberHandlerIterator);

                if (signalMemberHandlerIterator == signalMemberHandlerQueue_.end()) {
                    break;
                }
            }
        }
    }

    return connection_->removeSignalMemberHandler(_dbusSignalHandlerToken, _dbusSignalHandler);
}

void DBusProxy::getCurrentValueForSignalListener(
        const std::string &getMethodName,
        std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
        const uint32_t subscription) {

    availabilityMutex_.lock();
    if (availabilityStatus_ == AvailabilityStatus::AVAILABLE) {
        availabilityMutex_.unlock();

        DBusMessage message = createMethodCall(getMethodName, "");

        DBusProxyAsyncSignalMemberCallbackHandler<DBusProxy>::Delegate::FunctionType myFunc = std::bind(&DBusProxy::signalInitialValueCallback,
                this,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                std::placeholders::_4);
        DBusProxyAsyncSignalMemberCallbackHandler<DBusProxy>::Delegate delegate(shared_from_this(), myFunc);
        connection_->sendDBusMessageWithReplyAsync(
                message,
                DBusProxyAsyncSignalMemberCallbackHandler<DBusProxy>::create(delegate, dbusSignalHandler, subscription),
                &defaultCallInfo);
    } else {
        availabilityMutex_.unlock();
    }
}

void DBusProxy::freeDesktopGetCurrentValueForSignalListener(
        std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler,
    const uint32_t subscription,
    const std::string &interfaceName,
    const std::string &propertyName) {

    availabilityMutex_.lock();
    if (availabilityStatus_ == AvailabilityStatus::AVAILABLE) {
        availabilityMutex_.unlock();

        DBusAddress itsAddress(getDBusAddress());
        itsAddress.setInterface("org.freedesktop.DBus.Properties");
        DBusMessage _message = DBusMessage::createMethodCall(itsAddress, "Get", "ss");
        DBusOutputStream output(_message);
        const bool success = DBusSerializableArguments<const std::string, const std::string>
                                ::serialize(output, interfaceName, propertyName);
        if (success) {
            output.flush();
            DBusProxyAsyncSignalMemberCallbackHandler<DBusProxy>::Delegate::FunctionType myFunc = std::bind(&DBusProxy::signalInitialValueCallback,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3,
                    std::placeholders::_4);
            DBusProxyAsyncSignalMemberCallbackHandler<DBusProxy>::Delegate delegate(shared_from_this(), myFunc);

            connection_->sendDBusMessageWithReplyAsync(
                    _message,
                    DBusProxyAsyncSignalMemberCallbackHandler<DBusProxy>::create(delegate, dbusSignalHandler, subscription),
                    &defaultCallInfo);
        }
    } else {
        availabilityMutex_.unlock();
    }
}

} // namespace DBus
} // namespace CommonAPI
