// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cassert>
#include <sstream>

#include <CommonAPI/Utils.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>
#include <CommonAPI/DBus/DBusUtils.hpp>
#include <CommonAPI/DBus/DBusProxyAsyncSignalMemberCallbackHandler.hpp>
#include <CommonAPI/Logger.hpp>

namespace CommonAPI {
namespace DBus {

DBusProxyStatusEvent::DBusProxyStatusEvent(DBusProxy *_dbusProxy)
    : dbusProxy_(_dbusProxy) {
}

void DBusProxyStatusEvent::onListenerAdded(const Listener &_listener, const Subscription _subscription) {
    (void)_subscription;
    if (dbusProxy_->isAvailable())
        _listener(AvailabilityStatus::AVAILABLE);
}

DBusProxy::DBusProxy(const DBusAddress &_dbusAddress,
                     const std::shared_ptr<DBusProxyConnection> &_connection):
                DBusProxyBase(_dbusAddress, _connection),
                dbusProxyStatusEvent_(this),
                availabilityStatus_(AvailabilityStatus::UNKNOWN),
                interfaceVersionAttribute_(*this, "uu", "getInterfaceVersion"),
                dbusServiceRegistry_(DBusServiceRegistry::get(_connection)),
                signalMemberHandlerInfo_(3000)
{
}

void DBusProxy::init() {
    dbusServiceRegistrySubscription_ = dbusServiceRegistry_->subscribeAvailabilityListener(
                    getAddress().getAddress(),
                    std::bind(&DBusProxy::onDBusServiceInstanceStatus, this, std::placeholders::_1));
}

DBusProxy::~DBusProxy() {
    dbusServiceRegistry_->unsubscribeAvailabilityListener(
                    getAddress().getAddress(),
                    dbusServiceRegistrySubscription_);
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

ProxyStatusEvent& DBusProxy::getProxyStatusEvent() {
    return dbusProxyStatusEvent_;
}

InterfaceVersionAttribute& DBusProxy::getInterfaceVersionAttribute() {
    return interfaceVersionAttribute_;
}

void DBusProxy::signalMemberCallback(const CallStatus _status,
        const DBusMessage& dbusMessage,
        DBusProxyConnection::DBusSignalHandler *_handler,
        const uint32_t _tag) {
    (void)_status;
    (void)_tag;
    _handler->onSignalDBusMessage(dbusMessage);
}

void DBusProxy::signalInitialValueCallback(const CallStatus _status,
        const DBusMessage &_message,
        DBusProxyConnection::DBusSignalHandler *_handler,
        const uint32_t _tag) {
    (void)_status;
    _handler->onInitialValueSignalDBusMessage(_message, _tag);
}

void DBusProxy::onDBusServiceInstanceStatus(const AvailabilityStatus& availabilityStatus) {
    if (availabilityStatus != availabilityStatus_) {
        availabilityStatusMutex_.lock();
        availabilityStatus_ = availabilityStatus;
        availabilityStatusMutex_.unlock();

        dbusProxyStatusEvent_.notifyListeners(availabilityStatus);

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

                    DBusProxyAsyncSignalMemberCallbackHandler::FunctionType myFunc = std::bind(
                            &DBusProxy::signalMemberCallback,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2,
                            std::placeholders::_3,
                            std::placeholders::_4);
                    connection_->sendDBusMessageWithReplyAsync(
                            message,
                            DBusProxyAsyncSignalMemberCallbackHandler::create(myFunc, std::get<5>(*signalMemberHandlerIterator), 0),
                            &signalMemberHandlerInfo_);
                }
            }
            {
                std::lock_guard < std::mutex > queueLock(selectiveBroadcastHandlersMutex_);
                for (auto selectiveBroadcasts : selectiveBroadcastHandlers) {
                    std::string methodName = "subscribeFor" + selectiveBroadcasts.first + "Selective";
                    bool subscriptionAccepted = connection_->sendPendingSelectiveSubscription(this, methodName);
                    if (!subscriptionAccepted) {
                        selectiveBroadcasts.second->onError(CommonAPI::CallStatus::SUBSCRIPTION_REFUSED);
                    }
                }
            }
        } else {
            std::lock_guard < std::mutex > queueLock(signalMemberHandlerQueueMutex_);

            for(auto signalMemberHandlerIterator = signalMemberHandlerQueue_.begin();
                    signalMemberHandlerIterator != signalMemberHandlerQueue_.end();
                    signalMemberHandlerIterator++) {

                if (std::get<7>(*signalMemberHandlerIterator)) {
                    DBusProxyConnection::DBusSignalHandlerToken signalHandlerToken (
                            std::get<0>(*signalMemberHandlerIterator),
                            std::get<1>(*signalMemberHandlerIterator),
                            std::get<2>(*signalMemberHandlerIterator),
                            std::get<3>(*signalMemberHandlerIterator));
                    connection_->removeSignalMemberHandler(signalHandlerToken, std::get<5>(*signalMemberHandlerIterator));
                    std::get<7>(*signalMemberHandlerIterator) = false;
                }
            }
        }
    }
    availabilityStatusMutex_.lock();
    availabilityCondition_.notify_one();
    availabilityStatusMutex_.unlock();
}

DBusProxyConnection::DBusSignalHandlerToken DBusProxy::subscribeForSelectiveBroadcastOnConnection(
                                                      bool& subscriptionAccepted,
                                                      const std::string& objectPath,
                                                      const std::string& interfaceName,
                                                      const std::string& interfaceMemberName,
                                                      const std::string& interfaceMemberSignature,
                                                      DBusProxyConnection::DBusSignalHandler* dbusSignalHandler) {

    DBusProxyConnection::DBusSignalHandlerToken token =
            getDBusConnection()->subscribeForSelectiveBroadcast(
                    subscriptionAccepted,
                    objectPath,
                    interfaceName,
                    interfaceMemberName,
                    interfaceMemberSignature,
                    dbusSignalHandler,
                    this);

    if (!isAvailable()) {
        subscriptionAccepted = true;
    }
    if (subscriptionAccepted) {
        std::lock_guard < std::mutex > queueLock(selectiveBroadcastHandlersMutex_);
        selectiveBroadcastHandlers[interfaceMemberName] = dbusSignalHandler;
    }

    return token;
}

void DBusProxy::unsubscribeFromSelectiveBroadcast(const std::string& eventName,
                                                 DBusProxyConnection::DBusSignalHandlerToken subscription,
                                                 const DBusProxyConnection::DBusSignalHandler* dbusSignalHandler) {

    getDBusConnection()->unsubscribeFromSelectiveBroadcast(eventName, subscription, this, dbusSignalHandler);

    std::lock_guard < std::mutex > queueLock(selectiveBroadcastHandlersMutex_);
    std::string interfaceMemberName = std::get<2>(subscription);
    auto its_handler = selectiveBroadcastHandlers.find(interfaceMemberName);
    if (its_handler != selectiveBroadcastHandlers.end()) {
        selectiveBroadcastHandlers.erase(its_handler);
    }
}

DBusProxyConnection::DBusSignalHandlerToken DBusProxy::addSignalMemberHandler(
                const std::string& objectPath,
                const std::string& interfaceName,
                const std::string& signalName,
                const std::string& signalSignature,
                DBusProxyConnection::DBusSignalHandler* dbusSignalHandler,
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
        DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
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

        availabilityStatusMutex_.lock();
        if (availabilityStatus_ == AvailabilityStatus::AVAILABLE) {
            availabilityStatusMutex_.unlock();
            signalHandlerToken = connection_->addSignalMemberHandler(
                objectPath,
                interfaceName,
                signalName,
                signalSignature,
                dbusSignalHandler,
                justAddFilter);
            std::get<7>(signalMemberHandler) = true;
        } else {
            availabilityStatusMutex_.unlock();
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
            const DBusProxyConnection::DBusSignalHandler *_dbusSignalHandler) {

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
        DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
        const uint32_t subscription) {

    availabilityStatusMutex_.lock();
    if (availabilityStatus_ == AvailabilityStatus::AVAILABLE) {
        availabilityStatusMutex_.unlock();

        DBusMessage message = createMethodCall(getMethodName, "");

        DBusProxyAsyncSignalMemberCallbackHandler::FunctionType myFunc = std::bind(&DBusProxy::signalInitialValueCallback,
                this,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                std::placeholders::_4);
        connection_->sendDBusMessageWithReplyAsync(
                message,
                DBusProxyAsyncSignalMemberCallbackHandler::create(myFunc, dbusSignalHandler, subscription),
                &signalMemberHandlerInfo_);
    } else {
        availabilityStatusMutex_.unlock();
    }
}

void DBusProxy::freeDesktopGetCurrentValueForSignalListener(
    DBusProxyConnection::DBusSignalHandler *dbusSignalHandler,
    const uint32_t subscription,
    const std::string &interfaceName,
    const std::string &propertyName) {

    availabilityStatusMutex_.lock();
    if (availabilityStatus_ == AvailabilityStatus::AVAILABLE) {
        availabilityStatusMutex_.unlock();

        DBusAddress itsAddress(getDBusAddress());
        itsAddress.setInterface("org.freedesktop.DBus.Properties");
        DBusMessage _message = DBusMessage::createMethodCall(itsAddress, "Get", "ss");
        DBusOutputStream output(_message);
        const bool success = DBusSerializableArguments<const std::string, const std::string>
                                ::serialize(output, interfaceName, propertyName);
        if (success) {
            output.flush();
            DBusProxyAsyncSignalMemberCallbackHandler::FunctionType myFunc = std::bind(&DBusProxy::signalInitialValueCallback,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3,
                    std::placeholders::_4);

            connection_->sendDBusMessageWithReplyAsync(
                    _message,
                    DBusProxyAsyncSignalMemberCallbackHandler::create(myFunc, dbusSignalHandler, subscription),
                    &signalMemberHandlerInfo_);
        }
    } else {
        availabilityStatusMutex_.unlock();
    }
}


} // namespace DBus
} // namespace CommonAPI
