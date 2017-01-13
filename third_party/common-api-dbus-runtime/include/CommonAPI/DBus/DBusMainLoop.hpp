// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_DBUS_MAIN_LOOP_HPP_
#define COMMONAPI_DBUS_MAIN_LOOP_HPP_

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include <CommonAPI/MainLoopContext.hpp>

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

namespace CommonAPI {
namespace DBus {

typedef pollfd DBusMainLoopPollFd;

class DBusMainLoop {
 public:
    DBusMainLoop() = delete;
    DBusMainLoop(const DBusMainLoop&) = delete;
    DBusMainLoop& operator=(const DBusMainLoop&) = delete;
    DBusMainLoop(DBusMainLoop&&) = delete;
    DBusMainLoop& operator=(DBusMainLoop&&) = delete;

    COMMONAPI_EXPORT explicit DBusMainLoop(std::shared_ptr<MainLoopContext> context);
    COMMONAPI_EXPORT ~DBusMainLoop();

    /**
     * \brief Runs the mainloop indefinitely until stop() is called.
     *
     * Runs the mainloop indefinitely until stop() is called. The given timeout (milliseconds)
     * will be overridden if a timeout-event is present that defines an earlier ready time.
     */
    COMMONAPI_EXPORT void run(const int64_t& timeoutInterval = TIMEOUT_INFINITE);
    COMMONAPI_EXPORT void stop();

    /**
     * \brief Executes a single cycle of the mainloop.
     *
     * Subsequently calls prepare(), poll(), check() and, if necessary, dispatch().
     * The given timeout (milliseconds) represents the maximum time
     * this iteration will remain in the poll state. All other steps
     * are handled in a non-blocking way. Note however that a source
     * might claim to have infinite amounts of data to dispatch.
     * This demo-implementation of a Mainloop will dispatch a source
     * until it no longer claims to have data to dispatch.
     * Dispatch will not be called if no sources, watches and timeouts
     * claim to be ready during the check()-phase.
     *
     * @param timeout The maximum poll-timeout for this iteration.
     */
    COMMONAPI_EXPORT void doSingleIteration(const int64_t& timeout = TIMEOUT_INFINITE);

    /*
     * The given timeout is a maximum timeout in ms, measured from the current time in the future
     * (a value of 0 means "no timeout"). It will be overridden if a timeout-event is present
     * that defines an earlier ready time.
     */
    COMMONAPI_EXPORT bool prepare(const int64_t& timeout = TIMEOUT_INFINITE);
    COMMONAPI_EXPORT void poll();
    COMMONAPI_EXPORT bool check();
    COMMONAPI_EXPORT void dispatch();

 private:
    COMMONAPI_EXPORT void wakeup();
    COMMONAPI_EXPORT void wakeupAck();

    COMMONAPI_EXPORT void registerFileDescriptor(const DBusMainLoopPollFd& fileDescriptor);
    COMMONAPI_EXPORT void unregisterFileDescriptor(const DBusMainLoopPollFd& fileDescriptor);

    COMMONAPI_EXPORT void registerDispatchSource(DispatchSource* dispatchSource, const DispatchPriority dispatchPriority);
    COMMONAPI_EXPORT void unregisterDispatchSource(DispatchSource* dispatchSource);

    COMMONAPI_EXPORT void registerWatch(Watch* watch, const DispatchPriority dispatchPriority);
    COMMONAPI_EXPORT void unregisterWatch(Watch* watch);

    COMMONAPI_EXPORT void registerTimeout(Timeout* timeout, const DispatchPriority dispatchPriority);
    COMMONAPI_EXPORT void unregisterTimeout(Timeout* timeout);


    std::shared_ptr<MainLoopContext> context_;

    std::vector<DBusMainLoopPollFd> managedFileDescriptors_;
    std::mutex fileDescriptorsMutex_;

    struct DispatchSourceToDispatchStruct {
        DispatchSource* dispatchSource_;
        std::mutex* mutex_;
        bool isExecuted_; /* execution flag: indicates, whether the dispatchSource is dispatched currently */
        bool deleteObject_; /* delete flag: indicates, whether the dispatchSource can be deleted*/

        DispatchSourceToDispatchStruct(DispatchSource* _dispatchSource,
            std::mutex* _mutex,
            bool _isExecuted,
            bool _deleteObject) {
                dispatchSource_ = _dispatchSource;
                mutex_ = _mutex;
                isExecuted_ = _isExecuted;
                deleteObject_ = _deleteObject;
        }
    };

    struct TimeoutToDispatchStruct {
        Timeout* timeout_;
        std::mutex* mutex_;
        bool isExecuted_; /* execution flag: indicates, whether the timeout is dispatched currently */
        bool deleteObject_; /* delete flag: indicates, whether the timeout can be deleted*/
        bool timeoutElapsed_; /* timeout elapsed flag: indicates, whether the timeout is elapsed*/

        TimeoutToDispatchStruct(Timeout* _timeout,
            std::mutex* _mutex,
            bool _isExecuted,
            bool _deleteObject,
            bool _timeoutElapsed) {
                timeout_ = _timeout;
                mutex_ = _mutex;
                isExecuted_ = _isExecuted;
                deleteObject_ = _deleteObject;
                timeoutElapsed_ = _timeoutElapsed;
        }
    };

    struct WatchToDispatchStruct {
        int fd_;
        Watch* watch_;
        std::mutex* mutex_;
        bool isExecuted_; /* execution flag: indicates, whether the watch is dispatched currently */
        bool deleteObject_; /* delete flag: indicates, whether the watch can be deleted*/

        WatchToDispatchStruct(int _fd,
            Watch* _watch,
            std::mutex* _mutex,
            bool _isExecuted,
            bool _deleteObject) {
                fd_ = _fd;
                watch_ = _watch;
                mutex_ = _mutex;
                isExecuted_ = _isExecuted;
                deleteObject_ = _deleteObject;
        }
    };

    std::multimap<DispatchPriority, DispatchSourceToDispatchStruct*> registeredDispatchSources_;
    std::multimap<DispatchPriority, WatchToDispatchStruct*> registeredWatches_;
    std::multimap<DispatchPriority, TimeoutToDispatchStruct*> registeredTimeouts_;

    std::mutex dispatchSourcesMutex_;
    std::mutex watchesMutex_;
    std::mutex timeoutsMutex_;

    std::set<DispatchSourceToDispatchStruct*> sourcesToDispatch_;
    std::set<WatchToDispatchStruct*> watchesToDispatch_;
    std::set<TimeoutToDispatchStruct*> timeoutsToDispatch_;

    DispatchSourceListenerSubscription dispatchSourceListenerSubscription_;
    WatchListenerSubscription watchListenerSubscription_;
    TimeoutSourceListenerSubscription timeoutSourceListenerSubscription_;
    WakeupListenerSubscription wakeupListenerSubscription_;

    int64_t currentMinimalTimeoutInterval_;

    DBusMainLoopPollFd wakeFd_;

#ifdef WIN32
    DBusMainLoopPollFd sendFd_;
#endif

    bool hasToStop_;
    bool isBroken_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DEMO_MAIN_LOOP_HPP_
