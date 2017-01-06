// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/MainLoopContext.hpp>

namespace CommonAPI {

int64_t getCurrentTimeInMs() {
   return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

const std::string &MainLoopContext::getName() const {
    return name_;
}

DispatchSourceListenerSubscription MainLoopContext::subscribeForDispatchSources(DispatchSourceAddedCallback dispatchAddedCallback, DispatchSourceRemovedCallback dispatchRemovedCallback) {
    dispatchSourceListeners_.emplace_front(dispatchAddedCallback, dispatchRemovedCallback);
    return dispatchSourceListeners_.begin();
}

WatchListenerSubscription MainLoopContext::subscribeForWatches(WatchAddedCallback watchAddedCallback, WatchRemovedCallback watchRemovedCallback) {
    watchListeners_.emplace_front(watchAddedCallback, watchRemovedCallback);
    return watchListeners_.begin();
}

TimeoutSourceListenerSubscription MainLoopContext::subscribeForTimeouts(TimeoutSourceAddedCallback timeoutAddedCallback, TimeoutSourceRemovedCallback timeoutRemovedCallback) {
    timeoutSourceListeners_.emplace_front(timeoutAddedCallback, timeoutRemovedCallback);
    return timeoutSourceListeners_.begin();
}

WakeupListenerSubscription MainLoopContext::subscribeForWakeupEvents(WakeupCallback wakeupCallback) {
    wakeupListeners_.emplace_front(wakeupCallback);
    return wakeupListeners_.begin();
}

void MainLoopContext::unsubscribeForDispatchSources(DispatchSourceListenerSubscription subscription) {
    dispatchSourceListeners_.erase(subscription);
}

void MainLoopContext::unsubscribeForWatches(WatchListenerSubscription subscription) {
    watchListeners_.erase(subscription);
}

void MainLoopContext::unsubscribeForTimeouts(TimeoutSourceListenerSubscription subscription) {
    timeoutSourceListeners_.erase(subscription);
}

void MainLoopContext::unsubscribeForWakeupEvents(WakeupListenerSubscription subscription) {
    wakeupListeners_.erase(subscription);
}

void MainLoopContext::registerDispatchSource(DispatchSource* dispatchSource, const DispatchPriority dispatchPriority) {
    for(auto listener = dispatchSourceListeners_.begin(); listener != dispatchSourceListeners_.end(); ++listener) {
        listener->first(dispatchSource, dispatchPriority);
    }
}

void MainLoopContext::deregisterDispatchSource(DispatchSource* dispatchSource) {
    for(auto listener = dispatchSourceListeners_.begin(); listener != dispatchSourceListeners_.end(); ++listener) {
        listener->second(dispatchSource);
    }
}

void MainLoopContext::registerWatch(Watch* watch, const DispatchPriority dispatchPriority) {
    for(auto listener = watchListeners_.begin(); listener != watchListeners_.end(); ++listener) {
        listener->first(watch, dispatchPriority);
    }
}

void MainLoopContext::deregisterWatch(Watch* watch) {
    for(auto listener = watchListeners_.begin(); listener != watchListeners_.end(); ++listener) {
        listener->second(watch);
    }
}

void MainLoopContext::registerTimeoutSource(Timeout* timeoutEvent, const DispatchPriority dispatchPriority) {
    for(auto listener = timeoutSourceListeners_.begin(); listener != timeoutSourceListeners_.end(); ++listener) {
        listener->first(timeoutEvent, dispatchPriority);
    }
}

void MainLoopContext::deregisterTimeoutSource(Timeout* timeoutEvent) {
    for(auto listener = timeoutSourceListeners_.begin(); listener != timeoutSourceListeners_.end(); ++listener) {
        listener->second(timeoutEvent);
    }
}

void MainLoopContext::wakeup() {
    for(auto listener = wakeupListeners_.begin(); listener != wakeupListeners_.end(); ++listener) {
        (*listener)();
    }
}

bool MainLoopContext::isInitialized() {
    return dispatchSourceListeners_.size() > 0 || watchListeners_.size() > 0;
}

} // namespace CommonAPI
