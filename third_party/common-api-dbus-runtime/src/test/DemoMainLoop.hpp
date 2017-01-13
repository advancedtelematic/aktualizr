// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef DEMO_MAIN_LOOP_H_
#define DEMO_MAIN_LOOP_H_

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include <CommonAPI/MainLoopContext.hpp>

#include <vector>
#include <set>
#include <map>
#ifdef WIN32
#include <WinSock2.h>
#else
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif


#include <cassert>


namespace CommonAPI {

typedef pollfd DemoMainLoopPollFd;

class MainLoop {
 public:
    MainLoop() = delete;
    MainLoop(const MainLoop&) = delete;
    MainLoop& operator=(const MainLoop&) = delete;
    MainLoop(MainLoop&&) = delete;
    MainLoop& operator=(MainLoop&&) = delete;

    explicit MainLoop(std::shared_ptr<MainLoopContext> context) :
                      context_(context),
                      currentMinimalTimeoutInterval_(TIMEOUT_INFINITE),
                      breakLoop_(false),
                      running_(false){

#ifdef WIN32
        WSAEVENT wsaEvent = WSACreateEvent();

        if (wsaEvent != WSA_INVALID_EVENT) {
            wakeFd_.fd = PtrToInt(wsaEvent);
        }
#else
        wakeFd_.fd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
#endif
        wakeFd_.events = POLLIN;

        assert(wakeFd_.fd != -1);
        registerFileDescriptor(wakeFd_);

        dispatchSourceListenerSubscription_ = context_->subscribeForDispatchSources(
                std::bind(&CommonAPI::MainLoop::registerDispatchSource, this, std::placeholders::_1, std::placeholders::_2),
                std::bind(&CommonAPI::MainLoop::deregisterDispatchSource, this, std::placeholders::_1));
        watchListenerSubscription_ = context_->subscribeForWatches(
                std::bind(&CommonAPI::MainLoop::registerWatch, this, std::placeholders::_1, std::placeholders::_2),
                std::bind(&CommonAPI::MainLoop::deregisterWatch, this, std::placeholders::_1));
        timeoutSourceListenerSubscription_ = context_->subscribeForTimeouts(
                std::bind(&CommonAPI::MainLoop::registerTimeout, this, std::placeholders::_1, std::placeholders::_2),
                std::bind(&CommonAPI::MainLoop::deregisterTimeout, this, std::placeholders::_1));
        wakeupListenerSubscription_ = context_->subscribeForWakeupEvents(
                std::bind(&CommonAPI::MainLoop::wakeup, this));
    }

    ~MainLoop() {
        deregisterFileDescriptor(wakeFd_);

        context_->unsubscribeForDispatchSources(dispatchSourceListenerSubscription_);
        context_->unsubscribeForWatches(watchListenerSubscription_);
        context_->unsubscribeForTimeouts(timeoutSourceListenerSubscription_);
        context_->unsubscribeForWakeupEvents(wakeupListenerSubscription_);

#ifdef WIN32
        WSACloseEvent(IntToPtr(wakeFd_.fd));

#else
        close(wakeFd_.fd);
#endif
    }

    /**
     * \brief Runs the mainloop indefinitely until stop() is called.
     *
     * Runs the mainloop indefinitely until stop() is called. The given timeout (milliseconds)
     * will be overridden if a timeout-event is present that defines an earlier ready time.
     */
    void run(const int64_t& timeoutInterval = TIMEOUT_INFINITE) {
        running_ = true;
        while(running_) {
            doSingleIteration(timeoutInterval);
        }
    }

    void stop() {
        running_ = false;
        wakeup();
    }

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
    void doSingleIteration(const int64_t& timeout = TIMEOUT_INFINITE) {
        prepare(timeout);
        poll();
        if(check()) {
            dispatch();
        }
    }

    /*
     * The given timeout is a maximum timeout in ms, measured from the current time in the future
     * (a value of 0 means "no timeout"). It will be overridden if a timeout-event is present
     * that defines an earlier ready time.
     */
    void prepare(const int64_t& timeout = TIMEOUT_INFINITE) {
        currentMinimalTimeoutInterval_ = timeout;

        for (auto dispatchSourceIterator = registeredDispatchSources_.begin();
                        dispatchSourceIterator != registeredDispatchSources_.end();
                        dispatchSourceIterator++) {

            int64_t dispatchTimeout = TIMEOUT_INFINITE;
            if(dispatchSourceIterator->second->prepare(dispatchTimeout)) {
                sourcesToDispatch_.insert(*dispatchSourceIterator);
            } else if (dispatchTimeout < currentMinimalTimeoutInterval_) {
                currentMinimalTimeoutInterval_ = dispatchTimeout;
            }
        }

        int64_t currentContextTime = getCurrentTimeInMs();

        for (auto timeoutPriorityRange = registeredTimeouts_.begin();
                        timeoutPriorityRange != registeredTimeouts_.end();
                        timeoutPriorityRange++) {

            int64_t intervalToReady = timeoutPriorityRange->second->getReadyTime() - currentContextTime;

            if (intervalToReady <= 0) {
                timeoutsToDispatch_.insert(*timeoutPriorityRange);
                currentMinimalTimeoutInterval_ = TIMEOUT_NONE;
            } else if (intervalToReady < currentMinimalTimeoutInterval_) {
                currentMinimalTimeoutInterval_ = intervalToReady;
            }
        }
    }

    void poll() {
        for (auto fileDescriptor = managedFileDescriptors_.begin() + 1; fileDescriptor != managedFileDescriptors_.end(); ++fileDescriptor) {
            (*fileDescriptor).revents = 0;
        }

#if WIN32
        INT currentMinimalTimeoutIntervalWin32_ = 1;
        size_t numReadyFileDescriptors = ::WSAPoll(&(managedFileDescriptors_[0]), managedFileDescriptors_.size(), currentMinimalTimeoutIntervalWin32_);

        if (numReadyFileDescriptors == SOCKET_ERROR) {
            int iError = WSAGetLastError();
            //printf("WSAPoll failed with error: %ld\n", iError);
        }
#else
        size_t numReadyFileDescriptors = ::poll(&(managedFileDescriptors_[0]), managedFileDescriptors_.size(), int(currentMinimalTimeoutInterval_));
#endif
        // If no FileDescriptors are ready, poll returned because of a timeout that has expired.
        // The only case in which this is not the reason is when the timeout handed in "prepare"
        // expired before any other timeouts.
        if(!numReadyFileDescriptors) {
            int64_t currentContextTime = getCurrentTimeInMs();

            for (auto timeoutPriorityRange = registeredTimeouts_.begin();
                            timeoutPriorityRange != registeredTimeouts_.end();
                            timeoutPriorityRange++) {

                int64_t intervalToReady = timeoutPriorityRange->second->getReadyTime() - currentContextTime;

                if (intervalToReady <= 0) {
                    timeoutsToDispatch_.insert(*timeoutPriorityRange);
                }
            }
        }

        if (wakeFd_.revents) {
            acknowledgeWakeup();
        }
    }

    bool check() {
        //The first file descriptor always is the loop's wakeup-descriptor. All others need to be linked to a watch.
        for (auto fileDescriptor = managedFileDescriptors_.begin() + 1; fileDescriptor != managedFileDescriptors_.end(); ++fileDescriptor) {
            for (auto registeredWatchIterator = registeredWatches_.begin();
                        registeredWatchIterator != registeredWatches_.end();
                        registeredWatchIterator++) {
                const auto& correspondingWatchPriority = registeredWatchIterator->first;
                const auto& correspondingWatchPair = registeredWatchIterator->second;

                if (std::get<0>(correspondingWatchPair) == fileDescriptor->fd && fileDescriptor->revents) {
                    watchesToDispatch_.insert( { correspondingWatchPriority, {std::get<1>(correspondingWatchPair)} } );
                }
            }
        }

        for(auto dispatchSourceIterator = registeredDispatchSources_.begin(); dispatchSourceIterator != registeredDispatchSources_.end(); ++dispatchSourceIterator) {
            if((std::get<1>(*dispatchSourceIterator))->check()) {
                sourcesToDispatch_.insert( {std::get<0>(*dispatchSourceIterator), std::get<1>(*dispatchSourceIterator)});
            }
        }

        return !timeoutsToDispatch_.empty() || !watchesToDispatch_.empty() || !sourcesToDispatch_.empty();
    }

    void dispatch() {
        for (auto timeoutIterator = timeoutsToDispatch_.begin();
                timeoutIterator != timeoutsToDispatch_.end();
                timeoutIterator++) {
            std::get<1>(*timeoutIterator)->dispatch();
        }

        for (auto watchIterator = watchesToDispatch_.begin();
                watchIterator != watchesToDispatch_.end();
                watchIterator++) {
            Watch* watch = watchIterator->second;
            const unsigned int flags = watch->getAssociatedFileDescriptor().events;
            watch->dispatch(flags);
        }

        breakLoop_ = false;
        for (auto dispatchSourceIterator = sourcesToDispatch_.begin();
                        dispatchSourceIterator != sourcesToDispatch_.end() && !breakLoop_;
                        dispatchSourceIterator++) {

            while(std::get<1>(*dispatchSourceIterator)->dispatch());
        }

        timeoutsToDispatch_.clear();
        sourcesToDispatch_.clear();
        watchesToDispatch_.clear();
    }

    void wakeup() {
#ifdef WIN32
        HANDLE h = IntToPtr(wakeFd_.fd);
        SetEvent(h);
#else
        int64_t wake = 1;
        if(::write(wakeFd_.fd, &wake, sizeof(int64_t)) == -1) {
            std::perror("MainLoop::wakeup");
        }
#endif
    }

 private:
    void registerFileDescriptor(const DemoMainLoopPollFd& fileDescriptor) {
        managedFileDescriptors_.push_back(fileDescriptor);
    }

    void deregisterFileDescriptor(const DemoMainLoopPollFd& fileDescriptor) {
        for (auto it = managedFileDescriptors_.begin(); it != managedFileDescriptors_.end(); it++) {
            if ((*it).fd == fileDescriptor.fd) {
                managedFileDescriptors_.erase(it);
                break;
            }
        }
    }

    void registerDispatchSource(DispatchSource* dispatchSource, const DispatchPriority dispatchPriority) {
        registeredDispatchSources_.insert( {dispatchPriority, dispatchSource} );
    }

    void deregisterDispatchSource(DispatchSource* dispatchSource) {
        for(auto dispatchSourceIterator = registeredDispatchSources_.begin();
                dispatchSourceIterator != registeredDispatchSources_.end();
                dispatchSourceIterator++) {

            if(dispatchSourceIterator->second == dispatchSource) {
                registeredDispatchSources_.erase(dispatchSourceIterator);
                break;
            }
        }
        breakLoop_ = true;
    }

    void registerWatch(Watch* watch, const DispatchPriority dispatchPriority) {
#ifdef WIN32
        DemoMainLoopPollFd fdToRegister = watch->getAssociatedFileDescriptor();
#else
        DemoMainLoopPollFd fdToRegister = watch->getAssociatedFileDescriptor();
#endif

        registerFileDescriptor(fdToRegister);
        registeredWatches_.insert( {dispatchPriority, {watch->getAssociatedFileDescriptor().fd, watch}});
    }

    void deregisterWatch(Watch* watch) {
        deregisterFileDescriptor(watch->getAssociatedFileDescriptor());

        for(auto watchIterator = registeredWatches_.begin();
                watchIterator != registeredWatches_.end();
                watchIterator++) {

            if(watchIterator->second.second == watch) {
                registeredWatches_.erase(watchIterator);
                break;
            }
        }
    }

    void registerTimeout(Timeout* timeout, const DispatchPriority dispatchPriority) {
        registeredTimeouts_.insert( {dispatchPriority, timeout} );
    }

    void deregisterTimeout(Timeout* timeout) {
        for(auto timeoutIterator = registeredTimeouts_.begin();
                timeoutIterator != registeredTimeouts_.end();
                timeoutIterator++) {

            if(timeoutIterator->second == timeout) {
                registeredTimeouts_.erase(timeoutIterator);
                break;
            }
        }
    }

    void acknowledgeWakeup() {
#ifdef WIN32
        HANDLE h = IntToPtr(wakeFd_.fd);
        ResetEvent(h);
#else
        int64_t buffer;
        while (::read(wakeFd_.fd, &buffer, sizeof(int64_t)) == sizeof(buffer))
            ;
#endif
    }

    std::shared_ptr<MainLoopContext> context_;

    std::vector<DemoMainLoopPollFd> managedFileDescriptors_;

    std::multimap<DispatchPriority, DispatchSource*> registeredDispatchSources_;
    std::multimap<DispatchPriority, std::pair<int, Watch*>> registeredWatches_;
    std::multimap<DispatchPriority, Timeout*> registeredTimeouts_;

    std::set<std::pair<DispatchPriority, DispatchSource*>> sourcesToDispatch_;
    std::set<std::pair<DispatchPriority, Watch*>> watchesToDispatch_;
    std::set<std::pair<DispatchPriority, Timeout*>> timeoutsToDispatch_;

    DispatchSourceListenerSubscription dispatchSourceListenerSubscription_;
    WatchListenerSubscription watchListenerSubscription_;
    TimeoutSourceListenerSubscription timeoutSourceListenerSubscription_;
    WakeupListenerSubscription wakeupListenerSubscription_;

    int64_t currentMinimalTimeoutInterval_;
    bool breakLoop_;
    bool running_;

    DemoMainLoopPollFd wakeFd_;
};


} // namespace CommonAPI

#endif /* DEMO_MAIN_LOOP_H_ */
