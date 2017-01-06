// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef WIN32
#include <WinSock2.h>
#else
#include <poll.h>
#endif

#include <chrono>

#include <CommonAPI/DBus/DBusMainLoopContext.hpp>
#include <CommonAPI/DBus/DBusConnection.hpp>

namespace CommonAPI {
namespace DBus {

DBusDispatchSource::DBusDispatchSource(DBusConnection* dbusConnection):
    dbusConnection_(dbusConnection) {
}

DBusDispatchSource::~DBusDispatchSource() {
}

bool DBusDispatchSource::prepare(int64_t &_timeout) {
    (void)_timeout;
    return dbusConnection_->isDispatchReady();
}

bool DBusDispatchSource::check() {
    return dbusConnection_->isDispatchReady();
}

bool DBusDispatchSource::dispatch() {
    return dbusConnection_->singleDispatch();
}


DBusWatch::DBusWatch(::DBusWatch* libdbusWatch, std::weak_ptr<MainLoopContext>& mainLoopContext):
                libdbusWatch_(libdbusWatch),
                mainLoopContext_(mainLoopContext) {
    assert(libdbusWatch_);
}

bool DBusWatch::isReadyToBeWatched() {
    return 0 != dbus_watch_get_enabled(libdbusWatch_);
}

void DBusWatch::startWatching() {
    if(!dbus_watch_get_enabled(libdbusWatch_)) stopWatching();

    unsigned int channelFlags_ = dbus_watch_get_flags(libdbusWatch_);
#ifdef WIN32
    short int pollFlags = 0;
#else
    short int pollFlags = POLLERR | POLLHUP;
#endif
    if(channelFlags_ & DBUS_WATCH_READABLE) {
        pollFlags |= POLLIN;
    }
    if(channelFlags_ & DBUS_WATCH_WRITABLE) {
        pollFlags |= POLLOUT;
    }

#ifdef WIN32
    pollFileDescriptor_.fd = dbus_watch_get_socket(libdbusWatch_);
    wsaEvent_ = WSACreateEvent();
    WSAEventSelect(pollFileDescriptor_.fd, wsaEvent_, FD_READ);
#else
    pollFileDescriptor_.fd = dbus_watch_get_unix_fd(libdbusWatch_);
#endif

    pollFileDescriptor_.events = pollFlags;
    pollFileDescriptor_.revents = 0;

    auto lockedContext = mainLoopContext_.lock();
    assert(lockedContext);
    lockedContext->registerWatch(this);
}

void DBusWatch::stopWatching() {
    auto lockedContext = mainLoopContext_.lock();
    if (lockedContext) {
        lockedContext->deregisterWatch(this);
    }
}

const pollfd& DBusWatch::getAssociatedFileDescriptor() {
    return pollFileDescriptor_;
}

#ifdef WIN32
const HANDLE& DBusWatch::getAssociatedEvent() {
    return wsaEvent_;
}
#endif

void DBusWatch::dispatch(unsigned int eventFlags) {
#ifdef WIN32
    unsigned int dbusWatchFlags = 0;

    if (eventFlags & (POLLRDBAND | POLLRDNORM)) {
        dbusWatchFlags |= DBUS_WATCH_READABLE;
    }
    if (eventFlags & POLLWRNORM) {
        dbusWatchFlags |= DBUS_WATCH_WRITABLE;
    }
    if (eventFlags & (POLLERR | POLLNVAL)) {
        dbusWatchFlags |= DBUS_WATCH_ERROR;
    }
    if (eventFlags & POLLHUP) {
        dbusWatchFlags |= DBUS_WATCH_HANGUP;
    }
#else
    // Pollflags do not correspond directly to DBus watch flags
    unsigned int dbusWatchFlags = (eventFlags & POLLIN) |
                            ((eventFlags & POLLOUT) >> 1) |
                            ((eventFlags & POLLERR) >> 1) |
                            ((eventFlags & POLLHUP) >> 1);
#endif
    dbus_bool_t response = dbus_watch_handle(libdbusWatch_, dbusWatchFlags);

    if (!response) {
        printf("dbus_watch_handle returned FALSE!");
    }
}

const std::vector<DispatchSource*>& DBusWatch::getDependentDispatchSources() {
    return dependentDispatchSources_;
}

void DBusWatch::addDependentDispatchSource(DispatchSource* dispatchSource) {
    dependentDispatchSources_.push_back(dispatchSource);
}


DBusTimeout::DBusTimeout(::DBusTimeout* libdbusTimeout, std::weak_ptr<MainLoopContext>& mainLoopContext) :
                dueTimeInMs_(TIMEOUT_INFINITE),
                libdbusTimeout_(libdbusTimeout),
                mainLoopContext_(mainLoopContext) {
}

bool DBusTimeout::isReadyToBeMonitored() {
    return 0 != dbus_timeout_get_enabled(libdbusTimeout_);
}

void DBusTimeout::startMonitoring() {
    auto lockedContext = mainLoopContext_.lock();
    assert(lockedContext);
    recalculateDueTime();
    lockedContext->registerTimeoutSource(this);
}

void DBusTimeout::stopMonitoring() {
    dueTimeInMs_ = TIMEOUT_INFINITE;
    auto lockedContext = mainLoopContext_.lock();
    if (lockedContext) {
        lockedContext->deregisterTimeoutSource(this);
    }
}

bool DBusTimeout::dispatch() {
    recalculateDueTime();
    dbus_timeout_handle(libdbusTimeout_);
    return true;
}

int64_t DBusTimeout::getTimeoutInterval() const {
    return dbus_timeout_get_interval(libdbusTimeout_);
}

int64_t DBusTimeout::getReadyTime() const {
    return dueTimeInMs_;
}

void DBusTimeout::recalculateDueTime() {
    if(dbus_timeout_get_enabled(libdbusTimeout_)) {
        unsigned int intervalInMs = dbus_timeout_get_interval(libdbusTimeout_);
        dueTimeInMs_ = getCurrentTimeInMs() + intervalInMs;
    } else {
        dueTimeInMs_ = TIMEOUT_INFINITE;
    }
}

} // namespace DBus
} // namespace CommonAPI
