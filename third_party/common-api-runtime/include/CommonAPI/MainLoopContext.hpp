// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_MAINLOOPCONTEXT_HPP_
#define COMMONAPI_MAINLOOPCONTEXT_HPP_

#include <cstdint>

#ifdef WIN32
#include <WinSock2.h>
#else
#include <poll.h>
#endif

#ifdef WIN32
#undef max
#endif

#include <limits>
#include <vector>
#include <chrono>
#include <list>
#include <functional>
#include <string>

#include <CommonAPI/Export.hpp>

namespace CommonAPI {

enum class DispatchPriority {
    VERY_HIGH,
    HIGH,
    DEFAULT,
    LOW,
    VERY_LOW
};


int64_t COMMONAPI_EXPORT getCurrentTimeInMs();


/**
 * \brief Describes a basic element that periodically needs to be dispatched.
 *
 * A DispatchSource is not directly related to a file descriptor, but
 * may be dependent on a watch that manages a file descriptor. If this
 * is the case, the corresponding Watch will provide information about
 * which DispatchSources are dependent.
 */
struct DispatchSource {
    virtual ~DispatchSource() {}

    /**
     * Indicates whether this source is ready to be dispatched.
     * "Prepare" will be called before polling the file descriptors.
     *
     * @return 'true' if the source is ready to be dispatched.
     */
    virtual bool prepare(int64_t& timeout) = 0;

    /**
     * Indicates whether this source is ready to be dispatched.
     * "Check" will be called after polling the file descriptors.
     *
     * @return 'true' if the source is ready to be dispatched.
     */
    virtual bool check() = 0;

    /**
     * The return value indicates whether this dispatch source currently has
     * more data to dispatch. The mainloop may chose to ignore the return value.
     *
     * @return 'true' if there currently is more to dispatch, 'false' if not.
     */
    virtual bool dispatch() = 0;
};


/**
 * \brief Describes an element that manages a file descriptor.
 *
 * The watch is ready to be dispatched whenever it's managed file descriptor
 * has events in it's revents-field.
 *
 * It is possible that there are DispatchSources of which the dispatch readiness
 * directly depends on the dispatching of the watch. If this is the case, such
 * DispatchSources can be retrieved from this Watch.
 */
struct Watch {
    virtual ~Watch() {}

    /**
     * \brief Dispatches the watch.
     *
     * Should only be called once the associated file descriptor has events ready.
     *
     * @param eventFlags The events that shall be retrieved from the file descriptor.
     */
    virtual void dispatch(unsigned int eventFlags) = 0;

    /**
     * \brief Returns the file descriptor that is managed by this watch.
     *
     * @return The associated file descriptor.
     */
    virtual const pollfd& getAssociatedFileDescriptor() = 0;

#ifdef WIN32
    /**
    * \brief Returns the event bound to the file descriptor that is managed by this watch.
    *
    * @return The event bound to the associated file descriptor.
    */
    virtual const HANDLE& getAssociatedEvent() = 0;
#endif

    /**
     * \brief Returns a vector of all dispatch sources that depend on the watched file descriptor.
     *
     * The returned vector will not be empty if and only if there are any sources
     * that depend on availability of data of the watched file descriptor. Whenever this
     * Watch is dispatched, those sources likely also need to be dispatched.
     */
    virtual const std::vector<DispatchSource*>& getDependentDispatchSources() = 0;
};

const int64_t TIMEOUT_INFINITE = std::numeric_limits<int64_t>::max();
const int64_t TIMEOUT_NONE = 0;


/**
 * \brief Describes a basic timeout.
 *
 * Timeouts will be taken into consideration when waiting in a call to poll
 * for a file descriptor to become ready. When the lowest known timeout expires,
 * the call to poll will return, regardless of whether a file descriptor was ready
 * or not.
 */
struct Timeout {
    virtual ~Timeout() {}

    /**
     * Needs to be called when this timeout is expired.
     *
     * @return 'true' if the timeout shall be rescheduled, 'false' if it shall be removed.
     */
    virtual bool dispatch() = 0;

    /**
     * \brief The timeout interval in milliseconds.
     *
     * Returns TIMEOUT_INFINITE for "dispatch never", TIMEOUT_NONE for "dispatch immediately",
     * or any positive value as an interval of time in milliseconds that needs to pass before
     * this timeout is to be dispatched.
     */
    virtual int64_t getTimeoutInterval() const = 0;

    /**
     * \brief Returns the point in time at which this timeout needs to be dispatched next.
     *
     * After a initialization and after each dispatch, this timeout will re-calculate it's next
     * ready time. This value may be ignored if a different mechanism for monitoring timeout intervals
     * is used.
     */
    virtual int64_t getReadyTime() const = 0;
};


typedef std::function<void(DispatchSource*, const DispatchPriority)> DispatchSourceAddedCallback;
typedef std::function<void(DispatchSource*)> DispatchSourceRemovedCallback;
typedef std::function<void(Watch*, const DispatchPriority)> WatchAddedCallback;
typedef std::function<void(Watch*)> WatchRemovedCallback;
typedef std::function<void(Timeout*, const DispatchPriority)> TimeoutSourceAddedCallback;
typedef std::function<void(Timeout*)> TimeoutSourceRemovedCallback;
typedef std::function<void()> WakeupCallback;

typedef std::list<std::pair<DispatchSourceAddedCallback, DispatchSourceRemovedCallback>> DispatchSourceListenerList;
typedef std::list<std::pair<WatchAddedCallback, WatchRemovedCallback>> WatchListenerList;
typedef std::list<std::pair<TimeoutSourceAddedCallback, TimeoutSourceRemovedCallback>> TimeoutSourceListenerList;
typedef std::list<WakeupCallback> WakeupListenerList;

typedef DispatchSourceListenerList::iterator DispatchSourceListenerSubscription;
typedef WatchListenerList::iterator WatchListenerSubscription;
typedef TimeoutSourceListenerList::iterator TimeoutSourceListenerSubscription;
typedef WakeupListenerList::iterator WakeupListenerSubscription;


/**
 * \brief Provides hooks for your Main Loop implementation.
 *
 * By registering callbacks with this class, you will be notified about all DispatchSources,
 * Watches, Timeouts and Wakeup-Events that need to be handled by your Main Loop implementation.
 *
 */
class MainLoopContext {
public:
    COMMONAPI_EXPORT MainLoopContext(const std::string &_name = "COMMONAPI_DEFAULT_MAINLOOP_CONTEXT")
        : name_(_name){
    }

    COMMONAPI_EXPORT MainLoopContext(const MainLoopContext&) = delete;
    COMMONAPI_EXPORT MainLoopContext& operator=(const MainLoopContext&) = delete;
    COMMONAPI_EXPORT MainLoopContext(MainLoopContext&&) = delete;
    COMMONAPI_EXPORT MainLoopContext& operator=(MainLoopContext&&) = delete;

    COMMONAPI_EXPORT const std::string &getName() const;

    /**
     * \brief Registers for all DispatchSources that are added or removed.
     */
    COMMONAPI_EXPORT DispatchSourceListenerSubscription subscribeForDispatchSources(DispatchSourceAddedCallback dispatchAddedCallback, DispatchSourceRemovedCallback dispatchRemovedCallback);

    /**
     * \brief Registers for all Watches that are added or removed.
     */
    COMMONAPI_EXPORT WatchListenerSubscription subscribeForWatches(WatchAddedCallback watchAddedCallback, WatchRemovedCallback watchRemovedCallback);

    /**
     * \brief Registers for all Timeouts that are added or removed.
     */
    COMMONAPI_EXPORT TimeoutSourceListenerSubscription subscribeForTimeouts(TimeoutSourceAddedCallback timeoutAddedCallback, TimeoutSourceRemovedCallback timeoutRemovedCallback);

    /**
     * \brief Registers for all Wakeup-Events that need to interrupt a call to "poll".
     */
    COMMONAPI_EXPORT WakeupListenerSubscription subscribeForWakeupEvents(WakeupCallback wakeupCallback);

    /**
     * \brief Unsubscribes your listeners for DispatchSources.
     */
    COMMONAPI_EXPORT void unsubscribeForDispatchSources(DispatchSourceListenerSubscription subscription);

    /**
     * \brief Unsubscribes your listeners for Watches.
     */
    COMMONAPI_EXPORT void unsubscribeForWatches(WatchListenerSubscription subscription);

    /**
     * \brief Unsubscribes your listeners for Timeouts.
     */
    COMMONAPI_EXPORT void unsubscribeForTimeouts(TimeoutSourceListenerSubscription subscription);

    /**
     * \brief Unsubscribes your listeners for Wakeup-Events.
     */
    COMMONAPI_EXPORT void unsubscribeForWakeupEvents(WakeupListenerSubscription subscription);

    /**
     * \brief Notifies all listeners about a new DispatchSource.
     */
    COMMONAPI_EXPORT void registerDispatchSource(DispatchSource* dispatchSource, const DispatchPriority dispatchPriority = DispatchPriority::DEFAULT);

    /**
     * \brief Notifies all listeners about the removal of a DispatchSource.
     */
    COMMONAPI_EXPORT void deregisterDispatchSource(DispatchSource* dispatchSource);

    /**
     * \brief Notifies all listeners about a new Watch.
     */
    COMMONAPI_EXPORT void registerWatch(Watch* watch, const DispatchPriority dispatchPriority = DispatchPriority::DEFAULT);

    /**
     * \brief Notifies all listeners about the removal of a Watch.
     */
    COMMONAPI_EXPORT void deregisterWatch(Watch* watch);

    /**
     * \brief Notifies all listeners about a new Timeout.
     */
    COMMONAPI_EXPORT void registerTimeoutSource(Timeout* timeoutEvent, const DispatchPriority dispatchPriority = DispatchPriority::DEFAULT);

    /**
     * \brief Notifies all listeners about the removal of a Timeout.
     */
    COMMONAPI_EXPORT void deregisterTimeoutSource(Timeout* timeoutEvent);

    /**
     * \brief Notifies all listeners about a wakeup event that just happened.
     */
    COMMONAPI_EXPORT void wakeup();

    /**
     * \brief Will return true if at least one subscribe for DispatchSources or Watches has been called.
     *
     * This function will be used to prevent creation of a factory if a mainloop context is given, but
     * no listeners have been registered. This is done in order to ensure correct use of the mainloop context.
     */
    COMMONAPI_EXPORT bool isInitialized();

 private:
    DispatchSourceListenerList dispatchSourceListeners_;
    WatchListenerList watchListeners_;
    TimeoutSourceListenerList timeoutSourceListeners_;
    WakeupListenerList wakeupListeners_;

    std::string name_;
};

} // namespace CommonAPI

#endif // COMMONAPI_MAINLOOPCONTEXT_HPP_
