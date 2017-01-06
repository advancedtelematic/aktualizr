// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <chrono>
#include <future>
#include <sstream>
#include <thread>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/DBus/DBusConnection.hpp>
#include <CommonAPI/DBus/DBusInputStream.hpp>
#include <CommonAPI/DBus/DBusMainLoop.hpp>
#include <CommonAPI/DBus/DBusProxy.hpp>
#include <CommonAPI/DBus/DBusAddressTranslator.hpp>


namespace CommonAPI {
namespace DBus {

void MsgReplyQueueEntry::process(std::shared_ptr<DBusConnection> _connection) {
    _connection->dispatchDBusMessageReply(message_, replyAsyncHandler_);
}

void MsgReplyQueueEntry::clear() {
    delete replyAsyncHandler_;
}

void MsgQueueEntry::process(std::shared_ptr<DBusConnection> _connection) {
    (void)_connection;
}

void MsgQueueEntry::clear() {

}

DBusConnectionStatusEvent::DBusConnectionStatusEvent(DBusConnection* dbusConnection):
                dbusConnection_(dbusConnection) {
}

void DBusConnectionStatusEvent::onListenerAdded(const Listener &_listener, const Subscription _subscription) {
    (void)_subscription;
    if (dbusConnection_->isConnected())
        _listener(AvailabilityStatus::AVAILABLE);
}


const DBusObjectPathVTable* DBusConnection::getDBusObjectPathVTable() {
    static const DBusObjectPathVTable libdbusObjectPathVTable = {
                    NULL, // no need to handle unregister callbacks
                    &DBusConnection::onLibdbusObjectPathMessageThunk,
                    NULL, // dbus_internal_pad1 reserved for future expansion
                    NULL, // dbus_internal_pad2 reserved for future expansion
                    NULL, // dbus_internal_pad3 reserved for future expansion
                    NULL  // dbus_internal_pad4 reserved for future expansion
    };
    return &libdbusObjectPathVTable;
}

void DBusConnection::dispatch() {
    loop_->run();
}

DBusConnection::DBusConnection(DBusType_t busType,
                               const ConnectionId_t& _connectionId) :
                dispatchThread_(NULL),
                dispatchSource_(),
                watchContext_(NULL),
                timeoutContext_(NULL),
                connection_(NULL),
                busType_(busType),
                dbusConnectionStatusEvent_(this),
                libdbusSignalMatchRulesCount_(0),
                dbusObjectMessageHandler_(),
                connectionNameCount_(),
                enforcerThread_(NULL),
                enforcerThreadCancelled_(false),
                connectionId_(_connectionId),
                activeConnections_(0),
                isDisconnecting_(false),
                isDispatching_(false),
                isWaitingOnFinishedDispatching_(false) {

    dbus_threads_init_default();
}

DBusConnection::DBusConnection(::DBusConnection *_connection,
                               const ConnectionId_t& _connectionId) :
                dispatchThread_(NULL),
                dispatchSource_(),
                watchContext_(NULL),
                timeoutContext_(NULL),
                connection_(_connection),
                busType_(DBusType_t::WRAPPED),
                dbusConnectionStatusEvent_(this),
                libdbusSignalMatchRulesCount_(0),
                dbusObjectMessageHandler_(),
                connectionNameCount_(),
                enforcerThread_(NULL),
                enforcerThreadCancelled_(false),
                connectionId_(_connectionId),
                activeConnections_(0),
                isDisconnecting_(false),
                isDispatching_(false),
                isWaitingOnFinishedDispatching_(false) {

    dbus_threads_init_default();
}

void DBusConnection::setObjectPathMessageHandler(DBusObjectPathMessageHandler handler) {
    dbusObjectMessageHandler_ = handler;
}

bool DBusConnection::isObjectPathMessageHandlerSet() {
    return dbusObjectMessageHandler_.operator bool();
}

DBusConnection::~DBusConnection() {
    if (auto lockedContext = mainLoopContext_.lock()) {
        if (isConnected()) {
            dbus_connection_set_watch_functions(connection_, NULL, NULL, NULL, NULL, NULL);
            dbus_connection_set_timeout_functions(connection_, NULL, NULL, NULL, NULL, NULL);
        }

        lockedContext->deregisterDispatchSource(queueDispatchSource_);
        lockedContext->deregisterDispatchSource(dispatchSource_);
        lockedContext->deregisterWatch(queueWatch_);
        delete watchContext_;
        delete timeoutContext_;
    }

    //Assert that the enforcerThread_ is in a position to finish itself correctly even after destruction
    //of the DBusConnection. Also assert all resources are cleaned up.
    auto it = timeoutMap_.begin();
    while (it != timeoutMap_.end()) {
        DBusMessageReplyAsyncHandler* asyncHandler = std::get<1>(it->second);
        DBusPendingCall* libdbusPendingCall = it->first;

        asyncHandler->lock();
        bool executionStarted = asyncHandler->getExecutionStarted();
        bool executionFinished = asyncHandler->getExecutionFinished();
        if (executionStarted && !executionFinished) {
            asyncHandler->setHasToBeDeleted();
            it = timeoutMap_.erase(it);
            asyncHandler->unlock();
            continue;
        }
        if (!executionStarted && !executionFinished && !dbus_pending_call_get_completed(libdbusPendingCall)) {
            dbus_pending_call_cancel(libdbusPendingCall);
        }
        asyncHandler->unlock();

        if (!executionStarted && !executionFinished) {
            DBusMessage& dbusMessageCall = std::get<2>(it->second);

            asyncHandler->onDBusMessageReply(CallStatus::REMOTE_ERROR, dbusMessageCall.createMethodError(DBUS_ERROR_TIMEOUT));

            dbus_pending_call_unref(libdbusPendingCall);
        }
        it = timeoutMap_.erase(it);
        delete asyncHandler;
    }

    auto itTimeoutInf = timeoutInfiniteAsyncHandlers_.begin();
    while (itTimeoutInf != timeoutInfiniteAsyncHandlers_.end()) {
        DBusMessageReplyAsyncHandler* asyncHandler = (*itTimeoutInf);

        asyncHandler->lock();
        bool executionStarted = asyncHandler->getExecutionStarted();
        bool executionFinished = asyncHandler->getExecutionFinished();
        if (executionStarted && !executionFinished) {
            asyncHandler->setHasToBeDeleted();
            itTimeoutInf = timeoutInfiniteAsyncHandlers_.erase(itTimeoutInf);
            asyncHandler->unlock();
            continue;
        }
        asyncHandler->unlock();

        itTimeoutInf = timeoutInfiniteAsyncHandlers_.erase(itTimeoutInf);
        delete asyncHandler;
    }
}

bool DBusConnection::attachMainLoopContext(std::weak_ptr<MainLoopContext> mainLoopContext) {
    mainLoopContext_ = mainLoopContext;

    if (auto lockedContext = mainLoopContext_.lock()) {
        queueWatch_ = new DBusQueueWatch(shared_from_this());
        queueDispatchSource_ = new DBusQueueDispatchSource(queueWatch_);

        lockedContext->registerDispatchSource(queueDispatchSource_);
        lockedContext->registerWatch(queueWatch_);

        dispatchSource_ = new DBusDispatchSource(this);
        watchContext_ = new WatchContext(mainLoopContext_, dispatchSource_, shared_from_this());
        timeoutContext_ = new TimeoutContext(mainLoopContext_, shared_from_this());
        lockedContext->registerDispatchSource(dispatchSource_);

        if (!isConnected()) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), "not connected");
            return false;
        }

        dbus_connection_set_wakeup_main_function(
                        connection_,
                        &DBusConnection::onWakeupMainContext,
                        &mainLoopContext_,
                        NULL);

        bool success = 0 != dbus_connection_set_watch_functions(
                connection_,
                &DBusConnection::onAddWatch,
                &DBusConnection::onRemoveWatch,
                &DBusConnection::onToggleWatch,
                watchContext_,
                NULL);

        if (!success) {
            return false;
        }

        success = 0 != dbus_connection_set_timeout_functions(
                connection_,
                &DBusConnection::onAddTimeout,
                &DBusConnection::onRemoveTimeout,
                &DBusConnection::onToggleTimeout,
                timeoutContext_,
                NULL);

        if (!success) {
            dbus_connection_set_watch_functions(connection_, NULL, NULL, NULL, NULL, NULL);
            return false;
        }

        return true;
    }
    return false;
}

void DBusConnection::onWakeupMainContext(void* data) {
    std::weak_ptr<MainLoopContext>* mainloop = static_cast<std::weak_ptr<MainLoopContext>*>(data);

    if (!mainloop) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "mainloop == nullptr");
    } else if(auto lockedContext = mainloop->lock()) {
        lockedContext->wakeup();
    }
}

dbus_bool_t DBusConnection::onAddWatch(::DBusWatch* libdbusWatch, void* data) {
    WatchContext* watchContext = static_cast<WatchContext*>(data);
    if (NULL == watchContext) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "watchContext == NULL");
        return FALSE;
    } else {
        DBusWatch* dbusWatch = new DBusWatch(libdbusWatch, watchContext->mainLoopContext_, watchContext->dbusConnection_);
        dbusWatch->addDependentDispatchSource(watchContext->dispatchSource_);
        dbus_watch_set_data(libdbusWatch, dbusWatch, NULL);

        if (dbusWatch->isReadyToBeWatched()) {
            dbusWatch->startWatching();
        } else {
            delete dbusWatch;
            dbus_watch_set_data(libdbusWatch, NULL, NULL);
        }
    }

    return TRUE;
}

void DBusConnection::onRemoveWatch(::DBusWatch* libdbusWatch, void* data) {
    if (NULL == static_cast<WatchContext*>(data)) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "data (WatchContext) == NULL");
    }

    DBusWatch* dbusWatch = static_cast<DBusWatch*>(dbus_watch_get_data(libdbusWatch));
    if (dbusWatch != NULL) {
        if(dbusWatch->isReadyToBeWatched()) {
            dbusWatch->stopWatching();
        }
        dbus_watch_set_data(libdbusWatch, NULL, NULL);
    }
    // DBusWatch will be deleted in Mainloop
}

void DBusConnection::onToggleWatch(::DBusWatch* libdbusWatch, void* data) {
    WatchContext* watchContext = static_cast<WatchContext*>(data);

    if (NULL == watchContext) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "watchContext == NULL");
        return;
    }

    DBusWatch* dbusWatch = static_cast<DBusWatch*>(dbus_watch_get_data(libdbusWatch));

    if (dbusWatch == NULL) {
        DBusWatch* dbusWatch = new DBusWatch(libdbusWatch, watchContext->mainLoopContext_, watchContext->dbusConnection_);
        dbusWatch->addDependentDispatchSource(watchContext->dispatchSource_);
        dbus_watch_set_data(libdbusWatch, dbusWatch, NULL);

        if (dbusWatch->isReadyToBeWatched()) {
            dbusWatch->startWatching();
        }
    } else {
        if (!dbusWatch->isReadyToBeWatched()) {
            dbusWatch->stopWatching();
            dbus_watch_set_data(libdbusWatch, NULL, NULL);
        } else {
            dbusWatch->startWatching();
        }
    }
}


dbus_bool_t DBusConnection::onAddTimeout(::DBusTimeout* libdbusTimeout, void* data) {
    TimeoutContext* timeoutContext = static_cast<TimeoutContext*>(data);
    if (NULL == timeoutContext) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "timeoutContext == NULL");
        return FALSE;
    }

    DBusTimeout* dbusTimeout = new DBusTimeout(libdbusTimeout, timeoutContext->mainLoopContext_, timeoutContext->dbusConnection_);
    dbus_timeout_set_data(libdbusTimeout, dbusTimeout, NULL);

    if (dbusTimeout->isReadyToBeMonitored()) {
        dbusTimeout->startMonitoring();
    } else {
        delete dbusTimeout;
        dbus_timeout_set_data(libdbusTimeout, NULL, NULL);
    }

    return TRUE;
}

void DBusConnection::onRemoveTimeout(::DBusTimeout* libdbusTimeout, void* data) {
    if (NULL == static_cast<std::weak_ptr<MainLoopContext>*>(data)) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "MainLoopContext == NULL");
    }

    DBusTimeout* dbusTimeout = static_cast<DBusTimeout*>(dbus_timeout_get_data(libdbusTimeout));
    if (dbusTimeout) {
        dbusTimeout->stopMonitoring();
    }
    dbus_timeout_set_data(libdbusTimeout, NULL, NULL);
    // DBusTimeout will be deleted in Mainloop
}

void DBusConnection::onToggleTimeout(::DBusTimeout* dbustimeout, void* data) {
    if (NULL == static_cast<std::weak_ptr<MainLoopContext>*>(data)) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "MainLoopContext == NULL");
    }

    DBusTimeout* timeout = static_cast<DBusTimeout*>(dbus_timeout_get_data(dbustimeout));
    if (timeout->isReadyToBeMonitored()) {
        timeout->startMonitoring();
    } else {
        timeout->stopMonitoring();
    }
}

bool DBusConnection::connect(bool startDispatchThread) {
    DBusError dbusError;
    return connect(dbusError, startDispatchThread);
}

bool DBusConnection::connect(DBusError &dbusError, bool startDispatchThread) {
    if (dbusError) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "dbusError set");
        return false;
    }

    if (isConnected()) {
        return true;
    }

    const ::DBusBusType libdbusType = static_cast<DBusBusType>(busType_);

    connection_ = dbus_bus_get_private(libdbusType, &dbusError.libdbusError_);
    if (dbusError) {
        #ifdef _MSC_VER
                COMMONAPI_ERROR(std::string(__FUNCTION__) +
                    ": Name: " + dbusError.getName() +
                    " Message: " + dbusError.getMessage());
        #else
                COMMONAPI_ERROR(std::string(__PRETTY_FUNCTION__) +
                    ": Name: " + dbusError.getName() +
                    " Message: " + dbusError.getMessage());
        #endif
        return false;
    }

    if (NULL == connection_) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "connection_ == NULL");
        return false;
    }

    dbus_connection_set_exit_on_disconnect(connection_, false);

    initLibdbusObjectPathHandlerAfterConnect();

    initLibdbusSignalFilterAfterConnect();

    enforcerThread_ = std::make_shared<std::thread>(
                            std::bind(&DBusConnection::enforceAsynchronousTimeouts, shared_from_this()));

    dbusConnectionStatusEvent_.notifyListeners(AvailabilityStatus::AVAILABLE);

    isDisconnecting_ = false;
    if (startDispatchThread) {
        std::shared_ptr<MainLoopContext> itsContext = std::make_shared<MainLoopContext>();
        loop_ = std::make_shared<DBusMainLoop>(itsContext);
        attachMainLoopContext(itsContext);
        dispatchThread_ = new std::thread(std::bind(&DBusConnection::dispatch, shared_from_this()));
    }

    return true;
}

void DBusConnection::disconnect() {
    std::unique_lock<std::recursive_mutex> dbusConnectionLock(connectionGuard_);

    std::shared_ptr<DBusServiceRegistry> itsRegistry = DBusServiceRegistry::get(shared_from_this());

    isDisconnecting_ = true;

    if (std::shared_ptr<CommonAPI::MainLoopContext> mainLoopContext = mainLoopContext_.lock()) {
        Factory::get()->releaseConnection(connectionId_);
    }

    if (isConnected()) {
        dbusConnectionStatusEvent_.notifyListeners(AvailabilityStatus::NOT_AVAILABLE);

        if (libdbusSignalMatchRulesCount_ > 0) {
            dbus_connection_remove_filter(connection_, &onLibdbusSignalFilterThunk, this);
            libdbusSignalMatchRulesCount_ = 0;
        }

        connectionNameCount_.clear();

        //wait until dispatching is finished
        auto it = dispatchThreads_.find(std::this_thread::get_id());
        if(it == dispatchThreads_.end()) { //wait only if disconnect is NOT triggered by main loop
            while(isDispatching_) {
                isWaitingOnFinishedDispatching_ = true;
                dispatchCondition_.wait(dbusConnectionLock);
                isWaitingOnFinishedDispatching_ = false;
            }
        }
        dbusConnectionLock.unlock();

        dbus_connection_close(connection_);

        if(dispatchThread_) {
            loop_->stop();
            //It is possible for the disconnect to be called from within a callback, i.e. from within the dispatch
            //thread. Self-join is prevented this way.
            if (dispatchThread_->joinable() && std::this_thread::get_id() != dispatchThread_->get_id()) {
                dispatchThread_->join();
            } else {
                dispatchThread_->detach();
            }
            delete dispatchThread_;
            dispatchThread_ = NULL;
        }

        {
            std::lock_guard<std::recursive_mutex> enforcerLock(enforcerThreadMutex_);
            enforcerThreadCancelled_ = true;
            enforceTimeoutCondition_.notify_one();
        }

        if (enforcerThread_->joinable() &&
                std::this_thread::get_id() != enforcerThread_->get_id()) {
            enforcerThread_->join();
        } else {
            enforcerThread_->detach();
        }

        // remote mainloop watchers
        dbus_connection_set_watch_functions(connection_, NULL, NULL, NULL, NULL, NULL);
        dbus_connection_set_timeout_functions(connection_, NULL, NULL, NULL, NULL, NULL);

        dbus_connection_unref(connection_);
        connection_ = nullptr;
    }
}

bool DBusConnection::isConnected() const {
    return (connection_ != NULL);
}

DBusProxyConnection::ConnectionStatusEvent& DBusConnection::getConnectionStatusEvent() {
    return dbusConnectionStatusEvent_;
}

//Does this need to be a weak pointer?
const std::shared_ptr<DBusObjectManager> DBusConnection::getDBusObjectManager() {
    if (!dbusObjectManager_) {
        objectManagerGuard_.lock();
        if (!dbusObjectManager_) {
            dbusObjectManager_ = std::make_shared<DBusObjectManager>(shared_from_this());
        }
        objectManagerGuard_.unlock();
    }
    return dbusObjectManager_;
}

bool DBusConnection::requestServiceNameAndBlock(const std::string& serviceName) const {
    DBusError dbusError;
    bool isServiceNameAcquired = false;

    std::lock_guard<std::recursive_mutex> dbusConnectionLock(connectionGuard_);
    auto conIter = connectionNameCount_.find(serviceName);
    if (conIter == connectionNameCount_.end()) {
        const int libdbusStatus = dbus_bus_request_name(connection_,
                        serviceName.c_str(),
                        DBUS_NAME_FLAG_DO_NOT_QUEUE,
                        &dbusError.libdbusError_);

        isServiceNameAcquired = (libdbusStatus == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);
        if (isServiceNameAcquired) {
            connectionNameCount_.insert( { serviceName, (uint16_t)1 } );
        }
        else {
            if (libdbusStatus == -1) {
                #ifdef _MSC_VER // Visual Studio
                COMMONAPI_ERROR(std::string(__FUNCTION__) +
                    ": Name: " + dbusError.getName() +
                    " Message: " + dbusError.getMessage());
                #else
                COMMONAPI_ERROR(std::string(__PRETTY_FUNCTION__) +
                                ": Name: " + dbusError.getName() +
                                " Message: " + dbusError.getMessage());
                #endif
            }
        }
    } else {
        conIter->second++;
        isServiceNameAcquired = true;
    }

    return isServiceNameAcquired;
}

bool DBusConnection::releaseServiceName(const std::string& serviceName) const {
    DBusError dbusError;
    bool isServiceNameReleased = false;
    std::lock_guard<std::recursive_mutex> dbusConnectionLock(connectionGuard_);
    auto conIter = connectionNameCount_.find(serviceName);
    if (conIter != connectionNameCount_.end()) {
        if (conIter->second == 1) {
            const int libdbusStatus = dbus_bus_release_name(connection_,
                            serviceName.c_str(),
                            &dbusError.libdbusError_);
            isServiceNameReleased = (libdbusStatus == DBUS_RELEASE_NAME_REPLY_RELEASED);
            if (isServiceNameReleased) {
                connectionNameCount_.erase(conIter);
            }
        } else {
            conIter->second--;
            isServiceNameReleased = true;
        }
    }
    return isServiceNameReleased;
}

bool DBusConnection::sendDBusMessage(const DBusMessage &_message) const {
    if (!_message) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "message == NULL");
        return false;
    }
    if (!isConnected()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "not connected");
        return false;
    }

    dbus_uint32_t dbusSerial;
    const bool result = (0 != dbus_connection_send(connection_, _message.message_, &dbusSerial));
    return result;
}

DBusMessage DBusConnection::convertToDBusMessage(::DBusPendingCall* _libdbusPendingCall) {
    if (NULL == _libdbusPendingCall) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "_libdbusPendingCall == NULL");
        return DBusMessage();
    }

    ::DBusMessage* libdbusMessage = dbus_pending_call_steal_reply(_libdbusPendingCall);
    const bool increaseLibdbusMessageReferenceCount = false;
    DBusMessage dbusMessage(libdbusMessage, increaseLibdbusMessageReferenceCount);

    return dbusMessage;
}

void DBusConnection::onLibdbusPendingCall(::DBusPendingCall* _libdbusPendingCall,
                          const DBusMessage& _reply,
                          DBusMessageReplyAsyncHandler* _dbusMessageReplyAsyncHandler) const {

    CallStatus callStatus = CallStatus::SUCCESS;
    if (_reply.isErrorType() || !_reply.isMethodReturnType()) {
        if(strcmp(_reply.getError(), DBUS_ERROR_UNKNOWN_METHOD) == 0) {
            callStatus = CallStatus::NOT_AVAILABLE;
        } else {
            callStatus = CallStatus::REMOTE_ERROR;
        }
    }

    _dbusMessageReplyAsyncHandler->lock();
    bool processAsyncHandler = !_dbusMessageReplyAsyncHandler->getTimeoutOccurred();
    _dbusMessageReplyAsyncHandler->setExecutionStarted();
    _dbusMessageReplyAsyncHandler->unlock();

    if (processAsyncHandler)
        _dbusMessageReplyAsyncHandler->onDBusMessageReply(callStatus, _reply);

    _dbusMessageReplyAsyncHandler->lock();

    // libdbus calls the cleanup method below
    if(_libdbusPendingCall && processAsyncHandler) {
        dbus_pending_call_unref(_libdbusPendingCall);
        _dbusMessageReplyAsyncHandler->setExecutionFinished();
    }

    if (_dbusMessageReplyAsyncHandler->hasToBeDeleted()) {
        _dbusMessageReplyAsyncHandler->unlock();
        delete _dbusMessageReplyAsyncHandler;
    } else {
        _dbusMessageReplyAsyncHandler->unlock();
    }
}

void DBusConnection::onLibdbusPendingCallNotifyThunk(::DBusPendingCall* _libdbusPendingCall, void *_userData) {
    if (NULL == _libdbusPendingCall) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "_libdbusPendingCall == NULL");
        return;
    }
    if (NULL == _userData) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "_userData == NULL");
        return;
    }

    auto pendingCallNotificationData = reinterpret_cast<PendingCallNotificationData*>(_userData);
    auto dbusMessageReplyAsyncHandler = pendingCallNotificationData->replyAsyncHandler_;
    auto dbusConnection = pendingCallNotificationData->dbusConnection_;

    DBusMessage dbusMessage = DBusConnection::convertToDBusMessage(_libdbusPendingCall);

    dbusConnection->onLibdbusPendingCall(_libdbusPendingCall, dbusMessage, dbusMessageReplyAsyncHandler);
}

void DBusConnection::onLibdbusDataCleanup(void *_data) {
    auto pendingCallNotificationData = reinterpret_cast<PendingCallNotificationData*>(_data);
    delete pendingCallNotificationData;
}

//Would not be needed if libdbus would actually handle its timeouts for pending calls.
void DBusConnection::enforceAsynchronousTimeouts() const {
    std::unique_lock<std::recursive_mutex> itsLock(enforcerThreadMutex_);

    while (!enforcerThreadCancelled_) {
        enforceTimeoutMutex_.lock();

        int timeout = std::numeric_limits<int>::max(); // not really, but nearly "forever"
        if (timeoutMap_.size() > 0) {
            auto minTimeoutElement = std::min_element(timeoutMap_.begin(), timeoutMap_.end(),
                    [] (const TimeoutMapElement& lhs, const TimeoutMapElement& rhs) {
                        return std::get<0>(lhs.second) < std::get<0>(rhs.second);
            });

            auto minTimeout = std::get<0>(minTimeoutElement->second);

            std::chrono::steady_clock::time_point now = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();

            timeout = (int)std::chrono::duration_cast<std::chrono::milliseconds>(minTimeout - now).count();
        }

        enforceTimeoutMutex_.unlock();

        if (std::cv_status::timeout ==
            enforceTimeoutCondition_.wait_for(itsLock, std::chrono::milliseconds(timeout))) {

            //Do not access members if the DBusConnection was destroyed during the unlocked phase.
            enforceTimeoutMutex_.lock();
            auto it = timeoutMap_.begin();
            while (it != timeoutMap_.end()) {
                std::chrono::steady_clock::time_point now = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now();

                DBusMessageReplyAsyncHandler* asyncHandler = std::get<1>(it->second);
                DBusPendingCall* libdbusPendingCall = it->first;

                if (now > std::get<0>(it->second)) {

                    asyncHandler->lock();
                    bool executionStarted = asyncHandler->getExecutionStarted();
                    bool executionFinished = asyncHandler->getExecutionFinished();
                    if (!executionStarted && !executionFinished) {
                        asyncHandler->setTimeoutOccurred();
                        if (!dbus_pending_call_get_completed(libdbusPendingCall)) {
                            dbus_pending_call_cancel(libdbusPendingCall);
                        }
                    }
                    asyncHandler->unlock();

                    if (executionStarted && !executionFinished) {
                        // execution of asyncHandler is still running
                        // ==> add 100 ms for next timeout check
                        std::get<0>(it->second) = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
                    } else {
                        if (!executionFinished) {
                            // execution of asyncHandler was not finished (and not started)
                            // => add asyncHandler to mainloopTimeouts list
                            DBusMessage& dbusMessageCall = std::get<2>(it->second);

                            auto lockedContext = mainLoopContext_.lock();
                            if (!lockedContext) {
                                COMMONAPI_ERROR(std::string(__FUNCTION__), "lockedContext == nullptr");
                            } else {
                                {
                                    std::lock_guard<std::mutex> itsLock(mainloopTimeoutsMutex_);
                                    mainloopTimeouts_.push_back(std::make_tuple(asyncHandler,
                                        dbusMessageCall.createMethodError(DBUS_ERROR_TIMEOUT),
                                        CallStatus::REMOTE_ERROR,
                                        nullptr));
                                }
                                lockedContext->wakeup();
                            }
                            it = timeoutMap_.erase(it);

                            //This unref MIGHT cause the destruction of the last callback object that references the DBusConnection.
                            //So after this unref has been called, it has to be ensured that continuation of the loop is an option.
                            dbus_pending_call_unref(libdbusPendingCall);
                        } else {
                            // execution of asyncHandler was finished
                            it = timeoutMap_.erase(it);
                            delete asyncHandler;
                        }
                    }
                } else {
                    asyncHandler->lock();
                    bool executionFinished = asyncHandler->getExecutionFinished();
                    asyncHandler->unlock();
                    if (executionFinished) {
                        // execution of asyncHandler was finished but timeout is not expired
                        it = timeoutMap_.erase(it);
                        delete asyncHandler;
                    } else {
                        ++it;
                    }
                }
            }
            enforceTimeoutMutex_.unlock();
        } else {

            std::lock_guard<std::mutex> itsLock(enforceTimeoutMutex_);

            auto it = timeoutMap_.begin();
            while (it != timeoutMap_.end()) {
                DBusMessageReplyAsyncHandler* asyncHandler = std::get<1>(it->second);
                asyncHandler->lock();
                bool executionFinished = asyncHandler->getExecutionFinished();
                asyncHandler->unlock();
                if (executionFinished) {
                    // execution of asyncHandler was finished but timeout is not expired
                    it = timeoutMap_.erase(it);
                    delete asyncHandler;
                } else {
                    ++it;
                }
            }
        }

        {
            std::lock_guard<std::mutex> itsLock(timeoutInfiniteAsyncHandlersMutex_);
            // check for asyncHandler with infinite timeout whose execution is finished
            auto it = timeoutInfiniteAsyncHandlers_.begin();
            while (it != timeoutInfiniteAsyncHandlers_.end()) {
                DBusMessageReplyAsyncHandler* asyncHandler = (*it);
                asyncHandler->lock();
                bool executionFinished = asyncHandler->getExecutionFinished();
                asyncHandler->unlock();
                if ( executionFinished ) {
                    it = timeoutInfiniteAsyncHandlers_.erase(it);
                    delete asyncHandler;
                } else {
                    it++;
                }
            }
        }

    }
}

bool DBusConnection::sendDBusMessageWithReplyAsync(
        const DBusMessage& dbusMessage,
        std::unique_ptr<DBusMessageReplyAsyncHandler> dbusMessageReplyAsyncHandler,
        const CommonAPI::CallInfo *_info) const {

    std::lock_guard<std::recursive_mutex> dbusConnectionLock(connectionGuard_);

    if (!dbusMessage) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "message == NULL");
        return false;
    }
    if (!isConnected()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "not connected");
        return false;
    }

    DBusPendingCall* libdbusPendingCall;
    dbus_bool_t libdbusSuccess;

    DBusMessageReplyAsyncHandler* replyAsyncHandler = dbusMessageReplyAsyncHandler.release();

    PendingCallNotificationData* userData = new PendingCallNotificationData(this, replyAsyncHandler);
    DBusTimeout::currentTimeout_ = NULL;

    libdbusSuccess = dbus_connection_send_with_reply_set_notify(connection_,
            dbusMessage.message_,
            &libdbusPendingCall,
            onLibdbusPendingCallNotifyThunk,
            userData,
            onLibdbusDataCleanup,
            _info->timeout_);

    if (_info->sender_ != 0) {
        COMMONAPI_DEBUG("Message sent: SenderID: ", _info->sender_, " - Serial number: ", dbusMessage.getSerial());
    }

    if (!libdbusSuccess || !libdbusPendingCall) {
        #ifdef _MSC_VER // Visual Studio
            COMMONAPI_ERROR(std::string(__FUNCTION__) +
                ": (!libdbusSuccess || !libdbusPendingCall) == true");
        #else
            COMMONAPI_ERROR(std::string(__PRETTY_FUNCTION__) +
                            ": (!libdbusSuccess || !libdbusPendingCall) == true");
        #endif
        if (libdbusPendingCall) {
            dbus_pending_call_unref(libdbusPendingCall);
        }
        {
            std::lock_guard<std::mutex> itsLock(mainloopTimeoutsMutex_);
            mainloopTimeouts_.push_back(std::make_tuple(replyAsyncHandler,
                dbusMessage.createMethodError(DBUS_ERROR_DISCONNECTED),
                CallStatus::CONNECTION_FAILED,
                nullptr));
        }
        mainLoopContext_.lock()->wakeup();
        return true;
    }

    if (_info->timeout_ != DBUS_TIMEOUT_INFINITE) {
        auto timeoutPoint = (std::chrono::steady_clock::time_point) std::chrono::steady_clock::now() + std::chrono::milliseconds(_info->timeout_);
        std::tuple<
            std::chrono::steady_clock::time_point,
            DBusMessageReplyAsyncHandler*,
            DBusMessage> toInsert {
                timeoutPoint,
                replyAsyncHandler,
                dbusMessage
            };

        if(DBusTimeout::currentTimeout_)
            DBusTimeout::currentTimeout_->setPendingCall(libdbusPendingCall);

        enforceTimeoutMutex_.lock();
        auto ret = timeoutMap_.insert( { libdbusPendingCall, toInsert } );
        if (ret.second == false) {
            // key has been reused
            // update the map value with the new info
            DBusMessageReplyAsyncHandler* asyncHandler;
            auto it = timeoutMap_.find(ret.first->first);
            if(it != timeoutMap_.end()) {
                asyncHandler = std::get<1>(it->second);
                delete asyncHandler;
            }
            timeoutMap_.erase(ret.first);
            timeoutMap_.insert( { libdbusPendingCall, toInsert } );
        }
        enforceTimeoutMutex_.unlock();

        enforcerThreadMutex_.lock();
        enforceTimeoutCondition_.notify_one();
        enforcerThreadMutex_.unlock();
    } else {
        // add asyncHandler with infinite timeout to corresponding list
        std::lock_guard<std::mutex> itsLock(timeoutInfiniteAsyncHandlersMutex_);
        timeoutInfiniteAsyncHandlers_.insert(replyAsyncHandler);
    }

    return true;
}

DBusMessage DBusConnection::sendDBusMessageWithReplyAndBlock(const DBusMessage& dbusMessage,
                                                             DBusError& dbusError,
                                                             const CommonAPI::CallInfo *_info) const {
    if (!dbusMessage) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "message == NULL");
        return DBusMessage();
    }
    if (!isConnected()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "not connected");
        return DBusMessage();
    }

    ::DBusMessage* libdbusMessageReply = dbus_connection_send_with_reply_and_block(connection_,
                                                                                   dbusMessage.message_,
                                                                                   _info->timeout_,
                                                                                   &dbusError.libdbusError_);

    if (_info->sender_ != 0) {
        COMMONAPI_DEBUG("Message sent: SenderID: ", _info->sender_, " - Serial number: ", dbusMessage.getSerial());
    }

    const bool increaseLibdbusMessageReferenceCount = false;
    return DBusMessage(libdbusMessageReply, increaseLibdbusMessageReferenceCount);
}

void DBusConnection::dispatchDBusMessageReply(const DBusMessage& _reply,
                                              DBusMessageReplyAsyncHandler* _dbusMessageReplyAsyncHandler) {
    if(setDispatching(true)) {
        onLibdbusPendingCall(NULL, _reply, _dbusMessageReplyAsyncHandler);
        setDispatching(false);
    }
}

bool DBusConnection::singleDispatch() {
    std::list<MainloopTimeout_t> mainloopTimeouts;
    {
        mainloopTimeoutsMutex_.lock();
        for (auto t : mainloopTimeouts_) {
            mainloopTimeouts.push_back(t);
        }
        mainloopTimeouts_.clear();
        mainloopTimeoutsMutex_.unlock();
    }

    for (auto t : mainloopTimeouts) {
        std::get<0>(t)->onDBusMessageReply(std::get<2>(t), std::get<1>(t));
        if (std::get<3>(t) != nullptr) {
            dbus_pending_call_unref(std::get<3>(t));
        }
        delete std::get<0>(t);
    }

    if(setDispatching(true)) {
        bool dispatchStatus(connection_ && dbus_connection_dispatch(connection_) == DBUS_DISPATCH_DATA_REMAINS);
        setDispatching(false);
        return dispatchStatus;
    } else {
        return false;
    }
}

bool DBusConnection::isDispatchReady() {
    std::lock_guard<std::mutex> itsLock(mainloopTimeoutsMutex_);

    if(setDispatching(true)) {
        bool dispatchStatus((connection_ && dbus_connection_get_dispatch_status(connection_) == DBUS_DISPATCH_DATA_REMAINS) ||
            !mainloopTimeouts_.empty());
        setDispatching(false);
        return dispatchStatus;
    }
    return false;
}

bool DBusConnection::hasDispatchThread() {
    return (dispatchThread_ != NULL);
}

const ConnectionId_t& DBusConnection::getConnectionId() const {
    return connectionId_;
}

void DBusConnection::incrementConnection() {
    std::lock_guard < std::mutex > lock(activeConnectionsMutex_);
    activeConnections_++;
}

void DBusConnection::decrementConnection() {
    int activeConnections = 0;
    {
        std::lock_guard < std::mutex > lock(activeConnectionsMutex_);
        activeConnections = --activeConnections_;
    }

    if (activeConnections <= 0) {
        disconnect();
    }
}

bool DBusConnection::setDispatching(bool _isDispatching) {
    std::lock_guard<std::recursive_mutex> dispatchLock(connectionGuard_);

    if(isDispatching_ == _isDispatching)
        return true;

    dispatchThreads_.insert(std::this_thread::get_id());
    if(isDisconnecting_) { // we want to disconnect and only accept unsetting the dispatch flag
        if(!_isDispatching) {
            isDispatching_ = _isDispatching;
            if(isWaitingOnFinishedDispatching_)
                dispatchCondition_.notify_one();
            return true;
        } else {
            return false;
        }
    } else {
        isDispatching_ = _isDispatching;
        return true;
    }
}

void DBusConnection::sendPendingSelectiveSubscription(DBusProxy* callingProxy,
                                                      std::string interfaceMemberName,
                                                      std::weak_ptr<DBusSignalHandler> dbusSignalHandler,
                                                      uint32_t tag,
                                                      std::string interfaceMemberSignature) {
    bool outarg;
    std::string methodName = "subscribeFor" + interfaceMemberName + "Selective";
    DBusProxyHelper<CommonAPI::DBus::DBusSerializableArguments<>,
                    CommonAPI::DBus::DBusSerializableArguments<bool>>::callMethodAsync(
                    *callingProxy, methodName.c_str(), "",
                    &CommonAPI::DBus::defaultCallInfo,
                    [this, dbusSignalHandler, callingProxy, tag, interfaceMemberName, interfaceMemberSignature]
                     (const CommonAPI::CallStatus& callStatus, const bool& accepted) {

        if (callStatus == CommonAPI::CallStatus::SUCCESS && accepted) {
            if(auto itsHandler = dbusSignalHandler.lock())
                itsHandler->onSpecificError(CommonAPI::CallStatus::SUCCESS, tag);
        } else {
            const DBusSignalHandlerPath itsToken(callingProxy->getDBusAddress().getObjectPath(),
                                                callingProxy->getDBusAddress().getInterface(),
                                                interfaceMemberName,
                                                interfaceMemberSignature);
            if(auto itsHandler = dbusSignalHandler.lock()) {
                removeSignalMemberHandler(itsToken, itsHandler.get());
                itsHandler->onSpecificError(CommonAPI::CallStatus::SUBSCRIPTION_REFUSED, tag);
            }
        }
    }, std::make_tuple(outarg));
}

void DBusConnection::subscribeForSelectiveBroadcast(
                    const std::string& objectPath,
                    const std::string& interfaceName,
                    const std::string& interfaceMemberName,
                    const std::string& interfaceMemberSignature,
                    std::weak_ptr<DBusSignalHandler> dbusSignalHandler,
                    DBusProxy* callingProxy,
                    uint32_t tag) {

    if(auto itsHandler = dbusSignalHandler.lock()) {

        std::string methodName = "subscribeFor" + interfaceMemberName + "Selective";

        DBusSignalHandlerToken token = addSignalMemberHandler(
                                objectPath,
                                interfaceName,
                                interfaceMemberName,
                                interfaceMemberSignature,
                                dbusSignalHandler,
                                true
                            );

        itsHandler->setSubscriptionToken(token, tag);

        bool outarg;
        if(callingProxy->isAvailable()) {
            DBusProxyHelper<CommonAPI::DBus::DBusSerializableArguments<>,
                            CommonAPI::DBus::DBusSerializableArguments<bool>>::callMethodAsync(
                            *callingProxy, methodName.c_str(), "",
                            &CommonAPI::DBus::defaultCallInfo,
                            [this, objectPath, interfaceName, interfaceMemberName, interfaceMemberSignature, dbusSignalHandler, callingProxy, tag, token]
                             (const CommonAPI::CallStatus& callStatus, const bool& accepted) {
                (void)callStatus;
                if (accepted) {
                    if(auto itsHandler = dbusSignalHandler.lock())
                        itsHandler->onSpecificError(CommonAPI::CallStatus::SUCCESS, tag);
                } else {
                    if(auto itsHandler = dbusSignalHandler.lock()) {
                        removeSignalMemberHandler(token, itsHandler.get());
                        itsHandler->onSpecificError(CommonAPI::CallStatus::SUBSCRIPTION_REFUSED, tag);
                    }
                }
            }, std::make_tuple(outarg));
        }
    }
}

void DBusConnection::unsubscribeFromSelectiveBroadcast(const std::string& eventName,
                                                      DBusSignalHandlerToken subscription,
                                                      DBusProxy* callingProxy,
                                                      const DBusSignalHandler* dbusSignalHandler) {
    bool lastListenerOnConnectionRemoved = removeSignalMemberHandler(subscription, dbusSignalHandler);

    if (lastListenerOnConnectionRemoved) {
        // send unsubscribe message to stub
        std::string methodName = "unsubscribeFrom" + eventName + "Selective";
        CommonAPI::CallStatus callStatus;
        DBusProxyHelper<CommonAPI::DBus::DBusSerializableArguments<>,
                        CommonAPI::DBus::DBusSerializableArguments<>>::callMethodWithReply(
                        *callingProxy, methodName.c_str(), "", &CommonAPI::DBus::defaultCallInfo, callStatus);
    }
}

DBusProxyConnection::DBusSignalHandlerToken DBusConnection::addSignalMemberHandler(const std::string& objectPath,
                                                                                   const std::string& interfaceName,
                                                                                   const std::string& interfaceMemberName,
                                                                                   const std::string& interfaceMemberSignature,
                                                                                   std::weak_ptr<DBusSignalHandler> dbusSignalHandler,
                                                                                   const bool justAddFilter) {
    DBusSignalHandlerPath dbusSignalHandlerPath(
                    objectPath,
                    interfaceName,
                    interfaceMemberName,
                    interfaceMemberSignature);
    std::lock_guard < std::mutex > dbusSignalLock(signalGuard_);

    auto signalEntry = dbusSignalHandlerTable_.find(dbusSignalHandlerPath);
    const bool isFirstSignalMemberHandler = (signalEntry == dbusSignalHandlerTable_.end());
    auto itsHandler = dbusSignalHandler.lock();
    if (itsHandler && isFirstSignalMemberHandler) {
        addLibdbusSignalMatchRule(objectPath, interfaceName, interfaceMemberName, justAddFilter);

        std::map<DBusSignalHandler*, std::weak_ptr<DBusSignalHandler>> handlerList;
        handlerList[itsHandler.get()] = dbusSignalHandler;

        dbusSignalHandlerTable_.insert( {
            dbusSignalHandlerPath,
            std::make_pair(std::make_shared<std::recursive_mutex>(), std::move(handlerList))
        } );
    } else if (itsHandler && !isFirstSignalMemberHandler) {
        signalEntry->second.first->lock();
        signalEntry->second.second[itsHandler.get()] = dbusSignalHandler;
        signalEntry->second.first->unlock();
    }

    return dbusSignalHandlerPath;
}

bool DBusConnection::removeSignalMemberHandler(const DBusSignalHandlerToken &dbusSignalHandlerToken,
                                               const DBusSignalHandler* dbusSignalHandler) {
    bool lastHandlerRemoved = false;
    std::lock_guard < std::mutex > dbusSignalLock(signalGuard_);

    auto signalEntry = dbusSignalHandlerTable_.find(dbusSignalHandlerToken);
    if (signalEntry != dbusSignalHandlerTable_.end()) {

        signalEntry->second.first->lock();
        auto selectedHandler = signalEntry->second.second.find(const_cast<DBusSignalHandler*>(dbusSignalHandler));
        if (selectedHandler != signalEntry->second.second.end()) {
            signalEntry->second.second.erase(selectedHandler);
            lastHandlerRemoved = (signalEntry->second.second.empty());
        }
        signalEntry->second.first->unlock();
    }

    if (lastHandlerRemoved) {
        dbusSignalHandlerTable_.erase(signalEntry);
        removeLibdbusSignalMatchRule(std::get<0>(dbusSignalHandlerToken),
                std::get<1>(dbusSignalHandlerToken),
                std::get<2>(dbusSignalHandlerToken));
    }

    return lastHandlerRemoved;
}

bool DBusConnection::addObjectManagerSignalMemberHandler(const std::string& dbusBusName,
                                                         std::weak_ptr<DBusSignalHandler> dbusSignalHandler) {
    if (dbusBusName.length() < 2) {
        return false;
    }

    if(auto itsHandler = dbusSignalHandler.lock()) {
        std::lock_guard<std::mutex> dbusSignalLock(dbusObjectManagerSignalGuard_);

        auto dbusSignalMatchRuleIterator = dbusObjectManagerSignalMatchRulesMap_.find(dbusBusName);
        const bool isDBusSignalMatchRuleFound = (dbusSignalMatchRuleIterator != dbusObjectManagerSignalMatchRulesMap_.end());

        if (!isDBusSignalMatchRuleFound) {
            if (isConnected() && !addObjectManagerSignalMatchRule(dbusBusName)) {
                return false;
            }

            auto insertResult = dbusObjectManagerSignalMatchRulesMap_.insert({ dbusBusName, 0 });
            const bool isInsertSuccessful = insertResult.second;

            if (!isInsertSuccessful) {
                if (isConnected()) {
                    const bool isRemoveSignalMatchRuleSuccessful = removeObjectManagerSignalMatchRule(dbusBusName);
                    if (!isRemoveSignalMatchRuleSuccessful) {
                        COMMONAPI_ERROR(std::string(__FUNCTION__), " removeObjectManagerSignalMatchRule", dbusBusName, " failed");
                    }
                }
                return false;
            }

            dbusSignalMatchRuleIterator = insertResult.first;
        }

        size_t &dbusSignalMatchRuleReferenceCount = dbusSignalMatchRuleIterator->second;
        dbusSignalMatchRuleReferenceCount++;
        dbusObjectManagerSignalHandlerTable_.insert( { dbusBusName, std::make_pair(itsHandler.get(), dbusSignalHandler) } );
    }

    return true;
}

bool DBusConnection::removeObjectManagerSignalMemberHandler(const std::string& dbusBusName,
                                                            DBusSignalHandler* dbusSignalHandler) {
    if (dbusBusName.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " empty dbusBusName");
        return false;
    }

    std::lock_guard<std::mutex> dbusSignalLock(dbusObjectManagerSignalGuard_);

    auto dbusSignalMatchRuleIterator = dbusObjectManagerSignalMatchRulesMap_.find(dbusBusName);
    const bool isDBusSignalMatchRuleFound = (dbusSignalMatchRuleIterator != dbusObjectManagerSignalMatchRulesMap_.end());

    if (!isDBusSignalMatchRuleFound) {
        return true;
    }

    auto dbusObjectManagerSignalHandlerRange = dbusObjectManagerSignalHandlerTable_.equal_range(dbusBusName);
    auto dbusObjectManagerSignalHandlerIterator = std::find_if(
        dbusObjectManagerSignalHandlerRange.first,
        dbusObjectManagerSignalHandlerRange.second,
        [&](decltype(*dbusObjectManagerSignalHandlerRange.first)& it) { return it.second.first == dbusSignalHandler; });
    const bool isDBusSignalHandlerFound = (dbusObjectManagerSignalHandlerIterator != dbusObjectManagerSignalHandlerRange.second);
    if (!isDBusSignalHandlerFound) {
        return false;
    }

    size_t& dbusSignalMatchRuleReferenceCount = dbusSignalMatchRuleIterator->second;

    if (0 == dbusSignalMatchRuleReferenceCount) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "ref count == 0");
    } else {
        dbusSignalMatchRuleReferenceCount--;
    }

    const bool isLastDBusSignalMatchRuleReference = (dbusSignalMatchRuleReferenceCount == 0);
    if (isLastDBusSignalMatchRuleReference) {
        if (isConnected() && !removeObjectManagerSignalMatchRule(dbusBusName)) {
            return false;
        }

        dbusObjectManagerSignalMatchRulesMap_.erase(dbusSignalMatchRuleIterator);
    }

    dbusObjectManagerSignalHandlerTable_.erase(dbusObjectManagerSignalHandlerIterator);

    return true;
}

bool DBusConnection::addObjectManagerSignalMatchRule(const std::string& dbusBusName) {
    std::ostringstream dbusMatchRuleStringStream;
    dbusMatchRuleStringStream << "type='signal'"
                                << ",sender='" << dbusBusName << "'"
                                << ",interface='org.freedesktop.DBus.ObjectManager'";
    return addLibdbusSignalMatchRule(dbusMatchRuleStringStream.str());
}

bool DBusConnection::removeObjectManagerSignalMatchRule(const std::string& dbusBusName) {
    std::ostringstream dbusMatchRuleStringStream;
    dbusMatchRuleStringStream << "type='signal'"
                                << ",sender='" << dbusBusName << "'"
                                << ",interface='org.freedesktop.DBus.ObjectManager'";
    return removeLibdbusSignalMatchRule(dbusMatchRuleStringStream.str());
}

/**
 * Called only if connected
 *
 * @param dbusMatchRule
 * @return
 */
bool DBusConnection::addLibdbusSignalMatchRule(const std::string& dbusMatchRule) {
    bool libdbusSuccess = true;

    // add the libdbus message signal filter
    if (!libdbusSignalMatchRulesCount_) {
        libdbusSuccess = 0 != dbus_connection_add_filter(
            connection_,
            &onLibdbusSignalFilterThunk,
            this,
            NULL
        );
    }

    // finally add the match rule
    if (libdbusSuccess) {
        DBusError dbusError;
        dbus_bus_add_match(connection_, dbusMatchRule.c_str(), &dbusError.libdbusError_);
        libdbusSuccess = !dbusError;
    }

    if (libdbusSuccess) {
        libdbusSignalMatchRulesCount_++;
    }

    return libdbusSuccess;
}

/**
 * Called only if connected
 *
 * @param dbusMatchRule
 * @return
 */
bool DBusConnection::removeLibdbusSignalMatchRule(const std::string& dbusMatchRule) {
    if(libdbusSignalMatchRulesCount_ == 0)
        return true;

    dbus_bus_remove_match(connection_, dbusMatchRule.c_str(), NULL);

    libdbusSignalMatchRulesCount_--;
    if (libdbusSignalMatchRulesCount_ == 0) {
        dbus_connection_remove_filter(connection_, &onLibdbusSignalFilterThunk, this);
    }

    return true;
}

void DBusConnection::registerObjectPath(const std::string& objectPath) {
    if (objectPath.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " empty objectPath");
        return;
    }
    if ('/' != objectPath[0]) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " invalid objectPath ", objectPath);
        return;
    }

    auto handlerIterator = libdbusRegisteredObjectPaths_.find(objectPath);
    const bool foundRegisteredObjectPathHandler = handlerIterator != libdbusRegisteredObjectPaths_.end();

    if (foundRegisteredObjectPathHandler) {
        uint32_t& referenceCount = handlerIterator->second;
        referenceCount++;
        return;
    }

    libdbusRegisteredObjectPaths_.insert(LibdbusRegisteredObjectPathHandlersTable::value_type(objectPath, 1));

    if (isConnected()) {
        DBusError dbusError;
        const dbus_bool_t libdbusSuccess = dbus_connection_try_register_object_path(connection_,
                                                                                    objectPath.c_str(),
                                                                                    getDBusObjectPathVTable(),
                                                                                    this,
                                                                                    &dbusError.libdbusError_);

        if (!libdbusSuccess) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " dbus_connection_try_register_object_path failed for ", objectPath);
        }
        if (dbusError) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " name: ", dbusError.getName(), " message: ", dbusError.getMessage());
        }
    }
}

void DBusConnection::unregisterObjectPath(const std::string& objectPath) {
    if (objectPath.empty()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " empty objectPath");
        return;
    }
    if ('/' != objectPath[0]) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " invalid objectPath ", objectPath);
        return;
    }

    auto handlerIterator = libdbusRegisteredObjectPaths_.find(objectPath);
    const bool foundRegisteredObjectPathHandler = handlerIterator != libdbusRegisteredObjectPaths_.end();

    if (!foundRegisteredObjectPathHandler) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " no handler found for ", objectPath);
        return;
    }

    uint32_t& referenceCount = handlerIterator->second;
    if (referenceCount > 1) {
        referenceCount--;
        return;
    }

    libdbusRegisteredObjectPaths_.erase(handlerIterator);

    if (isConnected()) {
        const dbus_bool_t libdbusSuccess
            = dbus_connection_unregister_object_path(connection_, objectPath.c_str());
        if (!libdbusSuccess) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " dbus_connection_unregister_object_path failed for ", objectPath);
        }
    }
}

void DBusConnection::addLibdbusSignalMatchRule(const std::string& objectPath,
                                               const std::string& interfaceName,
                                               const std::string& interfaceMemberName,
                                               const bool justAddFilter) {
    DBusSignalMatchRuleTuple dbusSignalMatchRuleTuple(objectPath, interfaceName, interfaceMemberName);
    auto matchRuleIterator = dbusSignalMatchRulesMap_.find(dbusSignalMatchRuleTuple);
    const bool matchRuleFound = matchRuleIterator != dbusSignalMatchRulesMap_.end();

    if (matchRuleFound) {
        uint32_t& matchRuleReferenceCount = matchRuleIterator->second.first;
        matchRuleReferenceCount++;
        return;
    }

    const bool isFirstMatchRule = dbusSignalMatchRulesMap_.empty();

    // generate D-Bus match rule string
    std::ostringstream matchRuleStringStream;

    matchRuleStringStream << "type='signal'";
    matchRuleStringStream << ",path='" << objectPath << "'";
    matchRuleStringStream << ",interface='" << interfaceName << "'";
    matchRuleStringStream << ",member='" << interfaceMemberName << "'";

    // add the match rule string to the map with reference count set to 1
    std::string matchRuleString = matchRuleStringStream.str();
    auto success = dbusSignalMatchRulesMap_.insert(
                    DBusSignalMatchRulesMap::value_type(dbusSignalMatchRuleTuple,
                                    DBusSignalMatchRuleMapping(1, matchRuleString)));
    if (!success.second) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "dbusSignalMatchRulesMap_.insert failed ", matchRuleString);
        return;
    }

    if (isConnected()) {
        bool libdbusSuccess = true;

        // add the libdbus message signal filter
        if (isFirstMatchRule) {

            libdbusSuccess = 0 != dbus_connection_add_filter(
                                connection_,
                                &onLibdbusSignalFilterThunk,
                                this,
                                NULL);
            if (!libdbusSuccess) {
                COMMONAPI_ERROR(std::string(__FUNCTION__), " dbus_connection_add_filter() failed");
            }
        }

        if (!justAddFilter)
        {
            // finally add the match rule
            DBusError dbusError;
            dbus_bus_add_match(connection_, matchRuleString.c_str(), &dbusError.libdbusError_);

            if (dbusError) {
                COMMONAPI_ERROR(std::string(__FUNCTION__), " name: ", dbusError.getName(), " message: ", dbusError.getMessage());
            }
        }

        if (libdbusSuccess) {
            libdbusSignalMatchRulesCount_++;
        }
    }
}

void DBusConnection::removeLibdbusSignalMatchRule(const std::string& objectPath,
                                                  const std::string& interfaceName,
                                                  const std::string& interfaceMemberName) {
    auto selfReference = this->shared_from_this();
    DBusSignalMatchRuleTuple dbusSignalMatchRuleTuple(objectPath, interfaceName, interfaceMemberName);
    auto matchRuleIterator = dbusSignalMatchRulesMap_.find(dbusSignalMatchRuleTuple);
    const bool matchRuleFound = matchRuleIterator != dbusSignalMatchRulesMap_.end();

    if (!matchRuleFound) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " no match rule found for path: ", objectPath,
                            "interface: ", interfaceName, " member: ", interfaceMemberName);
    }

    uint32_t& matchRuleReferenceCount = matchRuleIterator->second.first;
    if (matchRuleReferenceCount > 1) {
        matchRuleReferenceCount--;
        return;
    }

    if (isConnected()) {
        const std::string& matchRuleString = matchRuleIterator->second.second;
        const bool libdbusSuccess = removeLibdbusSignalMatchRule(matchRuleString);
        if (!libdbusSuccess) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " removeLibdbusSignalMatchRule failed ", matchRuleString);
        }
    }

    dbusSignalMatchRulesMap_.erase(matchRuleIterator);
}

void DBusConnection::initLibdbusObjectPathHandlerAfterConnect() {
    if (!isConnected()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "not connected");
        return;
    }

    // nothing to do if there aren't any registered object path handlers
    if (libdbusRegisteredObjectPaths_.empty()) {
        return;
    }

    DBusError dbusError;
    dbus_bool_t libdbusSuccess;

    for (auto handlerIterator = libdbusRegisteredObjectPaths_.begin();
         handlerIterator != libdbusRegisteredObjectPaths_.end();
         handlerIterator++) {
        const std::string& objectPath = handlerIterator->first;

        dbusError.clear();

        libdbusSuccess = dbus_connection_try_register_object_path(connection_,
                                                                  objectPath.c_str(),
                                                                  getDBusObjectPathVTable(),
                                                                  this,
                                                                  &dbusError.libdbusError_);
        if (!libdbusSuccess) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " dbus_connection_try_register_object_path(", objectPath , ") failed ");
        }
        if (dbusError) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " name: ", dbusError.getName(), " message: ", dbusError.getMessage());
        }
    }
}

void DBusConnection::initLibdbusSignalFilterAfterConnect() {
    if (!isConnected()) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), "not connected");
        return;
    }

    // proxy/stub match rules
    for (const auto& dbusSignalMatchRuleIterator : dbusSignalMatchRulesMap_) {
        const auto& dbusSignalMatchRuleMapping = dbusSignalMatchRuleIterator.second;
        const std::string& dbusMatchRuleString = dbusSignalMatchRuleMapping.second;
        const bool libdbusSuccess = addLibdbusSignalMatchRule(dbusMatchRuleString);
        if (!libdbusSuccess) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " addLibdbusSignalMatchRule(", dbusMatchRuleString , ") failed ");
        }
    }

    // object manager match rules (see DBusServiceRegistry)
    for (const auto& dbusObjectManagerSignalMatchRuleIterator : dbusObjectManagerSignalMatchRulesMap_) {
        const std::string& dbusBusName = dbusObjectManagerSignalMatchRuleIterator.first;
        const bool libdbusSuccess = addObjectManagerSignalMatchRule(dbusBusName);
        if (!libdbusSuccess) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " addObjectManagerSignalMatchRule(", dbusBusName , ") failed ");
        }
    }
}

::DBusHandlerResult DBusConnection::onLibdbusObjectPathMessage(::DBusMessage* libdbusMessage) {
    if (NULL == libdbusMessage) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " libdbusMessage == NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // handle only method call messages
    if (dbus_message_get_type(libdbusMessage) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    bool isDBusMessageHandled = dbusObjectMessageHandler_(DBusMessage(libdbusMessage));
    return isDBusMessageHandled ? DBUS_HANDLER_RESULT_HANDLED : DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

template<typename DBusSignalHandlersTable>
void notifyDBusSignalHandlers(DBusSignalHandlersTable& dbusSignalHandlerstable,
                              typename DBusSignalHandlersTable::iterator& signalEntry,
                              const CommonAPI::DBus::DBusMessage& dbusMessage,
                              ::DBusHandlerResult& dbusHandlerResult) {
    if (signalEntry == dbusSignalHandlerstable.end() || signalEntry->second.second.empty()) {
        dbusHandlerResult = DBUS_HANDLER_RESULT_HANDLED;
        return;
    }

    auto handlerEntry = signalEntry->second.second.begin();
    while (handlerEntry != signalEntry->second.second.end()) {
        std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler = handlerEntry->second;
        if(auto itsHandler = dbusSignalHandler.lock())
            itsHandler->onSignalDBusMessage(dbusMessage);
        handlerEntry++;
    }
    dbusHandlerResult = DBUS_HANDLER_RESULT_HANDLED;
}

template<typename DBusSignalHandlersTable>
void notifyDBusOMSignalHandlers(DBusSignalHandlersTable& dbusSignalHandlerstable,
                              std::pair<typename DBusSignalHandlersTable::iterator,
                                        typename DBusSignalHandlersTable::iterator>& equalRange,
                              const CommonAPI::DBus::DBusMessage &dbusMessage,
                              ::DBusHandlerResult &dbusHandlerResult) {
    (void)dbusSignalHandlerstable;

    if (equalRange.first != equalRange.second) {
        dbusHandlerResult = DBUS_HANDLER_RESULT_HANDLED;
    }
    while (equalRange.first != equalRange.second) {
        std::weak_ptr<DBusProxyConnection::DBusSignalHandler> dbusSignalHandler = equalRange.first->second.second;
        if(auto itsHandler = dbusSignalHandler.lock())
            itsHandler->onSignalDBusMessage(dbusMessage);
        equalRange.first++;
    }
}

::DBusHandlerResult DBusConnection::onLibdbusSignalFilter(::DBusMessage* libdbusMessage) {
    if (NULL == libdbusMessage) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " libdbusMessage == NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    auto selfReference = this->shared_from_this();

    // handle only signal messages
    if (dbus_message_get_type(libdbusMessage) != DBUS_MESSAGE_TYPE_SIGNAL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* objectPath = dbus_message_get_path(libdbusMessage);
    const char* interfaceName = dbus_message_get_interface(libdbusMessage);
    const char* interfaceMemberName = dbus_message_get_member(libdbusMessage);
    const char* interfaceMemberSignature = dbus_message_get_signature(libdbusMessage);

    if (NULL == objectPath || NULL == interfaceName || NULL == interfaceMemberName || NULL == interfaceMemberSignature ) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " invalid message");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    DBusMessage dbusMessage(libdbusMessage);
    ::DBusHandlerResult dbusHandlerResult = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    signalGuard_.lock();
    auto signalEntry = dbusSignalHandlerTable_.find(DBusSignalHandlerPath(
        objectPath,
        interfaceName,
        interfaceMemberName,
        interfaceMemberSignature));

    if(signalEntry != dbusSignalHandlerTable_.end())
        signalEntry->second.first->lock();
    signalGuard_.unlock();

    // ensure, the registry survives
    std::shared_ptr<DBusServiceRegistry> itsRegistry_ = DBusServiceRegistry::get(shared_from_this());

    notifyDBusSignalHandlers(dbusSignalHandlerTable_,
                             signalEntry, dbusMessage, dbusHandlerResult);

    if(signalEntry != dbusSignalHandlerTable_.end())
        signalEntry->second.first->unlock();

    if (dbusMessage.hasInterfaceName("org.freedesktop.DBus.ObjectManager")) {
        const char* dbusSenderName = dbusMessage.getSender();
        if (NULL == dbusSenderName) {
            COMMONAPI_ERROR(std::string(__FUNCTION__), " dbusSenderName == NULL");
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        dbusObjectManagerSignalGuard_.lock();
        auto dbusObjectManagerSignalHandlerIteratorPair = dbusObjectManagerSignalHandlerTable_.equal_range(dbusSenderName);
        notifyDBusOMSignalHandlers(dbusObjectManagerSignalHandlerTable_,
                                 dbusObjectManagerSignalHandlerIteratorPair,
                                 dbusMessage,
                                 dbusHandlerResult);
        dbusObjectManagerSignalGuard_.unlock();
    }

    return dbusHandlerResult;
}

::DBusHandlerResult DBusConnection::onLibdbusSignalFilterThunk(::DBusConnection *_dbusConnection,
                                                               ::DBusMessage* libdbusMessage,
                                                               void* userData) {
    if (NULL == _dbusConnection) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " _dbusConnection == NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    if (NULL == libdbusMessage) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " libdbusMessage == NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    if (NULL == userData) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " userData == NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    DBusConnection* dbusConnection = reinterpret_cast<DBusConnection*>(userData);
    if (dbusConnection->connection_ != _dbusConnection) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " wrong connection!?");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    return dbusConnection->onLibdbusSignalFilter(libdbusMessage);
}

::DBusHandlerResult DBusConnection::onLibdbusObjectPathMessageThunk(::DBusConnection *_dbusConnection,
                                                                    ::DBusMessage* libdbusMessage,
                                                                    void* userData) {
    if (NULL == _dbusConnection) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " _dbusConnection == NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    if (NULL == libdbusMessage) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " libdbusMessage == NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    if (NULL == userData) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " userData == NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    DBusConnection* dbusConnection = reinterpret_cast<DBusConnection*>(userData);
    if (dbusConnection->connection_ != _dbusConnection) {
        COMMONAPI_ERROR(std::string(__FUNCTION__), " wrong connection!?");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    return dbusConnection->onLibdbusObjectPathMessage(libdbusMessage);
}

std::shared_ptr<DBusConnection> DBusConnection::getBus(const DBusType_t &_type, const ConnectionId_t& _connectionId) {
    return std::make_shared<DBusConnection>(_type, _connectionId);
}

std::shared_ptr<DBusConnection> DBusConnection::wrap(::DBusConnection *_connection, const ConnectionId_t& _connectionId) {
    return std::make_shared<DBusConnection>(_connection, _connectionId);
}

void DBusConnection::pushDBusMessageReplyToMainLoop(const DBusMessage& _reply,
                                  std::unique_ptr<DBusMessageReplyAsyncHandler> _dbusMessageReplyAsyncHandler) {
    // push message to the message queue
    DBusMessageReplyAsyncHandler* replyAsyncHandler = _dbusMessageReplyAsyncHandler.release();
    replyAsyncHandler->setHasToBeDeleted();
    std::shared_ptr<MsgReplyQueueEntry> msgReplyQueueEntry = std::make_shared<MsgReplyQueueEntry>(
            replyAsyncHandler, _reply);
    queueWatch_->pushQueue(msgReplyQueueEntry);
}

void DBusConnection::setPendingCallTimedOut(DBusPendingCall* _pendingCall, ::DBusTimeout* _timeout) const {
    std::lock_guard<std::mutex> lock(enforceTimeoutMutex_);
    auto it = timeoutMap_.find(_pendingCall);
    if(it != timeoutMap_.end()) {
        auto replyAsyncHandler = std::get<1>(it->second);
        replyAsyncHandler->lock();
        if(!replyAsyncHandler->getTimeoutOccurred()) {
            dbus_timeout_handle(_timeout);
        }
        replyAsyncHandler->unlock();
    }
}

} // namespace DBus
} // namespace CommonAPI
