// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSMAINLOOPCONTEXT_HPP_
#define COMMONAPI_DBUS_DBUSMAINLOOPCONTEXT_HPP_

#include <list>
#include <memory>

#include <dbus/dbus.h>

#include <CommonAPI/MainLoopContext.hpp>

namespace CommonAPI {
namespace DBus {

class DBusConnection;

class DBusDispatchSource: public DispatchSource {
 public:
    DBusDispatchSource(DBusConnection* dbusConnection);
    ~DBusDispatchSource();

    bool prepare(int64_t& timeout);
    bool check();
    bool dispatch();

 private:
    DBusConnection* dbusConnection_;
};

class DBusWatch: public Watch {
 public:
    DBusWatch(::DBusWatch* libdbusWatch, std::weak_ptr<MainLoopContext>& mainLoopContext);

    bool isReadyToBeWatched();
    void startWatching();
    void stopWatching();

    void dispatch(unsigned int eventFlags);

    const pollfd& getAssociatedFileDescriptor();

#ifdef WIN32
    const HANDLE& getAssociatedEvent();
#endif

    const std::vector<DispatchSource*>& getDependentDispatchSources();
    void addDependentDispatchSource(DispatchSource* dispatchSource);
 private:
    bool isReady();

    ::DBusWatch* libdbusWatch_;
    pollfd pollFileDescriptor_;
    std::vector<DispatchSource*> dependentDispatchSources_;

    std::weak_ptr<MainLoopContext> mainLoopContext_;

#ifdef WIN32
    HANDLE wsaEvent_;
#endif
};


class DBusTimeout: public Timeout {
 public:
    DBusTimeout(::DBusTimeout* libdbusTimeout, std::weak_ptr<MainLoopContext>& mainLoopContext);

    bool isReadyToBeMonitored();
    void startMonitoring();
    void stopMonitoring();

    bool dispatch();

    int64_t getTimeoutInterval() const;
    int64_t getReadyTime() const;
 private:
    void recalculateDueTime();

    int64_t dueTimeInMs_;
    ::DBusTimeout* libdbusTimeout_;
    std::weak_ptr<MainLoopContext> mainLoopContext_;
};


} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSMAINLOOPCONTEXT_HPP_
