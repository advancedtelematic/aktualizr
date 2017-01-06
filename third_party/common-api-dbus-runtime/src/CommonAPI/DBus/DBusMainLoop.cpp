// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <atomic>
#define DEFAULT_BUFLEN 512
#else
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif

#include <iostream>
#include <thread>
#include <cstdio>

#include <CommonAPI/DBus/DBusMainLoop.hpp>

namespace CommonAPI {
namespace DBus {

DBusMainLoop::DBusMainLoop(std::shared_ptr<MainLoopContext> context)
        : context_(context),
          currentMinimalTimeoutInterval_(TIMEOUT_INFINITE),
          hasToStop_(false),
          isBroken_(false) {
#ifdef WIN32
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, "0", &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
    }

    sockaddr* connected_addr = new sockaddr();
    USHORT port = 0;
    int namelength = sizeof(sockaddr);
    iResult = getsockname(ListenSocket, connected_addr, &namelength);
    if (iResult == SOCKET_ERROR) {
        printf("getsockname failed with error: %d\n", WSAGetLastError());
    } else if (connected_addr->sa_family == AF_INET) {
        port = ((struct sockaddr_in*)connected_addr)->sin_port;
    }
    delete connected_addr;

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
    }

    wsaData;
    wakeFd_.fd = INVALID_SOCKET;
    struct addrinfo *ptr = NULL;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo("127.0.0.1", std::to_string(ntohs(port)).c_str(), &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        wakeFd_.fd = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (wakeFd_.fd == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
        }

        // Connect to server.
        iResult = connect(wakeFd_.fd, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            printf("connect failed with error: %ld\n", WSAGetLastError());
            closesocket(wakeFd_.fd);
            wakeFd_.fd = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (wakeFd_.fd == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
    }

    // Accept a client socket
    sendFd_.fd = accept(ListenSocket, NULL, NULL);
    if (sendFd_.fd == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
    }

    wakeFd_.events = POLLIN;
    registerFileDescriptor(wakeFd_);
#else
    wakeFd_.fd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
    wakeFd_.events = POLLIN;
    registerFileDescriptor(wakeFd_);
#endif

    dispatchSourceListenerSubscription_ = context_->subscribeForDispatchSources(
            std::bind(&DBusMainLoop::registerDispatchSource, this,
                    std::placeholders::_1, std::placeholders::_2),
            std::bind(&DBusMainLoop::unregisterDispatchSource,
                    this, std::placeholders::_1));
    watchListenerSubscription_ = context_->subscribeForWatches(
            std::bind(&DBusMainLoop::registerWatch, this,
                    std::placeholders::_1, std::placeholders::_2),
            std::bind(&DBusMainLoop::unregisterWatch, this,
                    std::placeholders::_1));
    timeoutSourceListenerSubscription_ = context_->subscribeForTimeouts(
            std::bind(&DBusMainLoop::registerTimeout, this,
                    std::placeholders::_1, std::placeholders::_2),
            std::bind(&DBusMainLoop::unregisterTimeout, this,
                    std::placeholders::_1));
    wakeupListenerSubscription_ = context_->subscribeForWakeupEvents(
            std::bind(&DBusMainLoop::wakeup, this));
}

DBusMainLoop::~DBusMainLoop() {
    unregisterFileDescriptor (wakeFd_);

    context_->unsubscribeForDispatchSources(
            dispatchSourceListenerSubscription_);
    context_->unsubscribeForWatches(watchListenerSubscription_);
    context_->unsubscribeForTimeouts(timeoutSourceListenerSubscription_);
    context_->unsubscribeForWakeupEvents(wakeupListenerSubscription_);

#ifdef WIN32
    // shutdown the connection since no more data will be sent
    int iResult = shutdown(wakeFd_.fd, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(wakeFd_.fd);
        WSACleanup();
    }

    // cleanup
    closesocket(wakeFd_.fd);
    WSACleanup();
#else
    close(wakeFd_.fd);
#endif
}

void DBusMainLoop::run(const int64_t& timeoutInterval) {
    while (!hasToStop_) {
        doSingleIteration(timeoutInterval);
    }
}

void DBusMainLoop::stop() {
    hasToStop_ = true;
    wakeup();
}

void DBusMainLoop::doSingleIteration(const int64_t& timeout) {
    {
        std::lock_guard<std::mutex> itsLock(dispatchSourcesMutex_);
        for (auto dispatchSourceIterator = registeredDispatchSources_.begin();
            dispatchSourceIterator != registeredDispatchSources_.end();
            dispatchSourceIterator++) {

            (dispatchSourceIterator->second)->mutex_->lock();
            if ((dispatchSourceIterator->second)->deleteObject_) {
                if (!(dispatchSourceIterator->second)->isExecuted_) {
                    (dispatchSourceIterator->second)->mutex_->unlock();
                    bool contained = false;
                    for (std::set<DispatchSourceToDispatchStruct*>::iterator dispatchSourceIteratorInner = sourcesToDispatch_.begin();
                        dispatchSourceIteratorInner != sourcesToDispatch_.end(); dispatchSourceIteratorInner++) {
                        if ((*dispatchSourceIteratorInner)->dispatchSource_ == (dispatchSourceIterator->second)->dispatchSource_) {
                            contained = true;
                            break;
                        }
                    }
                    if (!contained) {
                        delete (dispatchSourceIterator->second)->dispatchSource_;
                        (dispatchSourceIterator->second)->dispatchSource_ = NULL;
                        delete (dispatchSourceIterator->second)->mutex_;
                        (dispatchSourceIterator->second)->mutex_ = NULL;
                        delete dispatchSourceIterator->second;
                        dispatchSourceIterator = registeredDispatchSources_.erase(dispatchSourceIterator);
                    }
                    if (dispatchSourceIterator == registeredDispatchSources_.end()) {
                        break;
                    }
                }
                else {
                    (dispatchSourceIterator->second)->mutex_->unlock();
                }
            }
            else {
                (dispatchSourceIterator->second)->mutex_->unlock();
            }
        }
    }

    {
        std::lock_guard<std::mutex> itsLock(timeoutsMutex_);
        for (auto timeoutIterator = registeredTimeouts_.begin();
            timeoutIterator != registeredTimeouts_.end();
            timeoutIterator++) {

            (timeoutIterator->second)->mutex_->lock();
            if ((timeoutIterator->second)->deleteObject_) {
                if (!(timeoutIterator->second)->isExecuted_) {
                    (timeoutIterator->second)->mutex_->unlock();
                    bool contained = false;
                    for (std::set<TimeoutToDispatchStruct*>::iterator timeoutIteratorInner = timeoutsToDispatch_.begin();
                        timeoutIteratorInner != timeoutsToDispatch_.end(); timeoutIteratorInner++) {
                        if ((*timeoutIteratorInner)->timeout_ == (timeoutIterator->second)->timeout_) {
                            contained = true;
                            break;
                        }
                    }
                    if (!contained) {
                        delete (timeoutIterator->second)->timeout_;
                        (timeoutIterator->second)->timeout_ = NULL;
                        delete (timeoutIterator->second)->mutex_;
                        (timeoutIterator->second)->mutex_ = NULL;
                        delete timeoutIterator->second;
                        timeoutIterator = registeredTimeouts_.erase(timeoutIterator);
                    }
                    if (timeoutIterator == registeredTimeouts_.end()) {
                        break;
                    }
                }
                else {
                    (timeoutIterator->second)->mutex_->unlock();
                }
            }
            else {
                (timeoutIterator->second)->mutex_->unlock();
            }
        }
    }

    {
        std::lock_guard<std::mutex> itsLock(watchesMutex_);
        for (auto watchesIterator = registeredWatches_.begin();
            watchesIterator != registeredWatches_.end();
            watchesIterator++) {

            (watchesIterator->second)->mutex_->lock();
            if ((watchesIterator->second)->deleteObject_) {
                if (!(watchesIterator->second)->isExecuted_) {
                    (watchesIterator->second)->mutex_->unlock();
                    bool contained = false;
                    for (auto watchesIteratorInner = watchesToDispatch_.begin();
                        watchesIteratorInner != watchesToDispatch_.end(); watchesIteratorInner++) {
                        if ((*watchesIteratorInner)->watch_ == (watchesIterator->second)->watch_) {
                            contained = true;
                            break;
                        }
                    }
                    if (!contained) {
                        delete (watchesIterator->second)->watch_;
                        (watchesIterator->second)->watch_ = NULL;
                        delete (watchesIterator->second)->mutex_;
                        (watchesIterator->second)->mutex_ = NULL;
                        delete watchesIterator->second;
                        watchesIterator = registeredWatches_.erase(watchesIterator);
                    }
                    if (watchesIterator == registeredWatches_.end()) {
                        break;
                    }
                }
                else {
                    (watchesIterator->second)->mutex_->unlock();
                }
            }
            else {
                (watchesIterator->second)->mutex_->unlock();
            }
        }
    }

    if (prepare(timeout)) {
        dispatch();
    } else {
        poll();
        if (check()) {
            dispatch();
        }
    }
}

bool DBusMainLoop::prepare(const int64_t& timeout) {
    currentMinimalTimeoutInterval_ = timeout;

    {
        std::lock_guard<std::mutex> itsLock(dispatchSourcesMutex_);
        for (auto dispatchSourceIterator = registeredDispatchSources_.begin();
                dispatchSourceIterator != registeredDispatchSources_.end();
                dispatchSourceIterator++) {

            int64_t dispatchTimeout = TIMEOUT_INFINITE;
            if ((dispatchSourceIterator->second)->dispatchSource_->prepare(dispatchTimeout)) {
                sourcesToDispatch_.insert(dispatchSourceIterator->second);
            } else if (dispatchTimeout < currentMinimalTimeoutInterval_) {
                currentMinimalTimeoutInterval_ = dispatchTimeout;
            }
        }
    }

    int64_t currentContextTime = getCurrentTimeInMs();

    {
        std::lock_guard<std::mutex> itsLock(timeoutsMutex_);
        for (auto timeoutPriorityRange = registeredTimeouts_.begin();
                timeoutPriorityRange != registeredTimeouts_.end();
                timeoutPriorityRange++) {

            (timeoutPriorityRange->second)->mutex_->lock();
            bool deleteObject = (timeoutPriorityRange->second)->deleteObject_;
            (timeoutPriorityRange->second)->mutex_->unlock();

            if (!deleteObject) {
                if (!(timeoutPriorityRange->second)->timeoutElapsed_) { // check that timeout is not elapsed
                    int64_t intervalToReady = (timeoutPriorityRange->second)->timeout_->getReadyTime()
                        - currentContextTime;

                    if (intervalToReady <= 0) {
                        // set information that timeout is elapsed
                        (timeoutPriorityRange->second)->timeoutElapsed_ = true;

                        timeoutsToDispatch_.insert(timeoutPriorityRange->second);
                        currentMinimalTimeoutInterval_ = TIMEOUT_NONE;
                    } else if (intervalToReady < currentMinimalTimeoutInterval_) {
                        currentMinimalTimeoutInterval_ = intervalToReady;
                    }
                }
            }
        }
    }

    return (!sourcesToDispatch_.empty() || !timeoutsToDispatch_.empty());
}

void DBusMainLoop::poll() {
    int managedFileDescriptorOffset = 0;

    for (auto fileDescriptor = managedFileDescriptors_.begin() + managedFileDescriptorOffset; fileDescriptor != managedFileDescriptors_.end(); ++fileDescriptor) {
        (*fileDescriptor).revents = 0;
    }

#ifdef WIN32    
    size_t numReadyFileDescriptors = WSAPoll(&managedFileDescriptors_[0], managedFileDescriptors_.size(), -1);
#else
    size_t numReadyFileDescriptors = ::poll(&(managedFileDescriptors_[0]),
            managedFileDescriptors_.size(), -1);
#endif
    if (!numReadyFileDescriptors) {
        int64_t currentContextTime = getCurrentTimeInMs();

        {
            std::lock_guard<std::mutex> itsLock(timeoutsMutex_);
            for (auto timeoutPriorityRange = registeredTimeouts_.begin();
                    timeoutPriorityRange != registeredTimeouts_.end();
                    timeoutPriorityRange++) {

                (timeoutPriorityRange->second)->mutex_->lock();
                bool deleteObject = (timeoutPriorityRange->second)->deleteObject_;
                (timeoutPriorityRange->second)->mutex_->unlock();

                if (!deleteObject) {
                    if (!(timeoutPriorityRange->second)->timeoutElapsed_) { // check that timeout is not elapsed
                        int64_t intervalToReady =
                            (timeoutPriorityRange->second)->timeout_->getReadyTime()
                                - currentContextTime;

                        if (intervalToReady <= 0) {
                            // set information that timeout is elapsed
                            (timeoutPriorityRange->second)->timeoutElapsed_ = true;

                            timeoutsToDispatch_.insert(timeoutPriorityRange->second);
                        }
                    }
                }
            }
        }
    }

    // If the wakeup descriptor woke us up, we must acknowledge
    if (managedFileDescriptors_[0].revents) {
        wakeupAck();
    }
}

bool DBusMainLoop::check() {
//The first file descriptor always is the loop's wakeup-descriptor (but not for windows anymore). All others need to be linked to a watch.
    int managedFileDescriptorOffset = 1;
    {
        std::lock_guard<std::mutex> itsLock(fileDescriptorsMutex_);
        for (auto fileDescriptor = managedFileDescriptors_.begin() + managedFileDescriptorOffset;
                fileDescriptor != managedFileDescriptors_.end(); ++fileDescriptor) {
            {
                std::lock_guard<std::mutex> itsWatchesLock(watchesMutex_);
                for (auto registeredWatchIterator = registeredWatches_.begin();
                        registeredWatchIterator != registeredWatches_.end();
                        registeredWatchIterator++) {
                    
                    (registeredWatchIterator->second)->mutex_->lock();
                    bool deleteObject = (registeredWatchIterator->second)->deleteObject_;
                    (registeredWatchIterator->second)->mutex_->unlock();

                    if (!deleteObject) {
                        if ((registeredWatchIterator->second)->fd_ == fileDescriptor->fd
                                && fileDescriptor->revents) {
                            watchesToDispatch_.insert(registeredWatchIterator->second);
                        }
                    }
                }
            }
        }
    }
    {
        std::lock_guard<std::mutex> itsLock(dispatchSourcesMutex_);
        for (auto dispatchSourceIterator = registeredDispatchSources_.begin();
                dispatchSourceIterator != registeredDispatchSources_.end();
                ++dispatchSourceIterator) {
            if ((dispatchSourceIterator->second)->dispatchSource_->check()) {
                sourcesToDispatch_.insert(dispatchSourceIterator->second);
            }
        }
    }

    return (!timeoutsToDispatch_.empty() ||
            !watchesToDispatch_.empty() ||
            !sourcesToDispatch_.empty());
}

void DBusMainLoop::dispatch() {
    if (timeoutsToDispatch_.size() > 0)
    {
        for (auto timeoutIterator = timeoutsToDispatch_.begin();
                timeoutIterator != timeoutsToDispatch_.end(); timeoutIterator++) {
            (*timeoutIterator)->mutex_->lock();
            if (!(*timeoutIterator)->deleteObject_) {
                (*timeoutIterator)->isExecuted_ = true;
                (*timeoutIterator)->mutex_->unlock();
                (*timeoutIterator)->timeout_->dispatch();
                (*timeoutIterator)->mutex_->lock();
                (*timeoutIterator)->isExecuted_ = false;
            }
            (*timeoutIterator)->mutex_->unlock();
        }

        timeoutsToDispatch_.clear();
    }

    if (watchesToDispatch_.size() > 0)
    {
        for (auto watchIterator = watchesToDispatch_.begin();
                watchIterator != watchesToDispatch_.end(); watchIterator++) {
            (*watchIterator)->mutex_->lock();
            if (!(*watchIterator)->deleteObject_) {
                (*watchIterator)->isExecuted_ = true;
                (*watchIterator)->mutex_->unlock();
                Watch* watch = (*watchIterator)->watch_;
                const unsigned int flags = watch->getAssociatedFileDescriptor().events;
                watch->dispatch(flags);
                (*watchIterator)->mutex_->lock();
                (*watchIterator)->isExecuted_ = false;
            }
            (*watchIterator)->mutex_->unlock();
        }
        watchesToDispatch_.clear();
    }

    if (sourcesToDispatch_.size() > 0)
    {
        isBroken_ = false;
        for (auto dispatchSourceIterator = sourcesToDispatch_.begin();
                dispatchSourceIterator != sourcesToDispatch_.end() && !isBroken_;
                dispatchSourceIterator++) {
            (*dispatchSourceIterator)->mutex_->lock();
            if (!(*dispatchSourceIterator)->deleteObject_) {
                (*dispatchSourceIterator)->isExecuted_ = true;
                (*dispatchSourceIterator)->mutex_->unlock();
                while ((*dispatchSourceIterator)->dispatchSource_->dispatch());
                (*dispatchSourceIterator)->mutex_->lock();
                (*dispatchSourceIterator)->isExecuted_ = false;
            }
            (*dispatchSourceIterator)->mutex_->unlock();
        }
        {
            sourcesToDispatch_.clear();
        }
    }
}

void DBusMainLoop::wakeup() {
#ifdef WIN32
    // Send an initial buffer
    char *sendbuf = "1";

    int iResult = send(sendFd_.fd, sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR) {
        int error = WSAGetLastError();

        if (error != WSANOTINITIALISED) {
            printf("send failed with error: %d\n", error);
        }
    }
#else
    int64_t wake = 1;
    if(::write(wakeFd_.fd, &wake, sizeof(int64_t)) == -1) {
        std::perror("DBusMainLoop::wakeup");
    }
#endif
}

void DBusMainLoop::wakeupAck() {
#ifdef WIN32
    // Receive until the peer closes the connection
    int iResult;    
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    iResult = recv(wakeFd_.fd, recvbuf, recvbuflen, 0);
    if (iResult > 0) {
        //printf("Bytes received from %d: %d\n", wakeFd_.fd, iResult);
    }
    else if (iResult == 0) {
        printf("Connection closed\n");
    }
    else {
        printf("recv failed with error: %d\n", WSAGetLastError());
    }

#else
    int64_t buffer;
    while(::read(wakeFd_.fd, &buffer, sizeof(int64_t)) == sizeof(buffer));
#endif
}

void DBusMainLoop::registerFileDescriptor(
        const DBusMainLoopPollFd& fileDescriptor) {
    std::lock_guard<std::mutex> itsLock(fileDescriptorsMutex_);
    managedFileDescriptors_.push_back(fileDescriptor);
}

void DBusMainLoop::unregisterFileDescriptor(
        const DBusMainLoopPollFd& fileDescriptor) {
    wakeup();
    std::lock_guard<std::mutex> itsLock(fileDescriptorsMutex_);
    for (auto it = managedFileDescriptors_.begin();
        it != managedFileDescriptors_.end(); it++) {
        if ((*it).fd == fileDescriptor.fd && (*it).events == fileDescriptor.events) {
            managedFileDescriptors_.erase(it);
            break;
        }
    }
}

void DBusMainLoop::registerDispatchSource(DispatchSource* dispatchSource,
        const DispatchPriority dispatchPriority) {
    DispatchSourceToDispatchStruct* dispatchSourceStruct = new DispatchSourceToDispatchStruct(dispatchSource, new std::mutex, false, false);
    std::lock_guard<std::mutex> itsLock(dispatchSourcesMutex_);
    registeredDispatchSources_.insert({ dispatchPriority, dispatchSourceStruct });
}

void DBusMainLoop::unregisterDispatchSource(DispatchSource* dispatchSource) {
    {
        std::lock_guard<std::mutex> itsLock(dispatchSourcesMutex_);
        for (auto dispatchSourceIterator = registeredDispatchSources_.begin();
            dispatchSourceIterator != registeredDispatchSources_.end();
            dispatchSourceIterator++) {

            if ((dispatchSourceIterator->second)->dispatchSource_ == dispatchSource){
                (dispatchSourceIterator->second)->mutex_->lock();
                (dispatchSourceIterator->second)->deleteObject_ = true;
                (dispatchSourceIterator->second)->mutex_->unlock();
                break;
            }
        }
        isBroken_ = true;
    }
}

void DBusMainLoop::registerWatch(Watch* watch,
        const DispatchPriority dispatchPriority) {

    std::lock_guard<std::mutex> itsLock(watchesMutex_);
    DBusMainLoopPollFd fdToRegister = watch->getAssociatedFileDescriptor();

    registerFileDescriptor(fdToRegister);
    std::mutex* mtx = new std::mutex;
#ifdef WIN32
    std::atomic_signal_fence(std::memory_order_acq_rel);
#else
    asm volatile ("":::"memory");
#endif
    WatchToDispatchStruct* watchStruct = new WatchToDispatchStruct(fdToRegister.fd, watch, mtx, false, false);
    registeredWatches_.insert({ dispatchPriority, watchStruct});
}

void DBusMainLoop::unregisterWatch(Watch* watch) {
    unregisterFileDescriptor(watch->getAssociatedFileDescriptor());
    {
        std::lock_guard<std::mutex> itsLock(watchesMutex_);

        for (auto watchIterator = registeredWatches_.begin();
                watchIterator != registeredWatches_.end(); watchIterator++) {

            if ((watchIterator->second)->watch_ == watch) {
                (watchIterator->second)->mutex_->lock();
                (watchIterator->second)->deleteObject_ = true;
                (watchIterator->second)->mutex_->unlock();
                break;
            }
        }
    }
}

void DBusMainLoop::registerTimeout(Timeout* timeout,
        const DispatchPriority dispatchPriority) {
    TimeoutToDispatchStruct* timeoutStruct = new TimeoutToDispatchStruct(timeout, new std::mutex, false, false, false);
    std::lock_guard<std::mutex> itsLock(timeoutsMutex_);
    registeredTimeouts_.insert({ dispatchPriority, timeoutStruct });
}

void DBusMainLoop::unregisterTimeout(Timeout* timeout) {
    {
        std::lock_guard<std::mutex> itsLock(timeoutsMutex_);
        for (auto timeoutIterator = registeredTimeouts_.begin();
                timeoutIterator != registeredTimeouts_.end();
                timeoutIterator++) {

            if ((timeoutIterator->second)->timeout_ == timeout) {
                (timeoutIterator->second)->mutex_->lock();
                (timeoutIterator->second)->deleteObject_ = true;
                (timeoutIterator->second)->mutex_->unlock();
                break;
            }
        }
    }
}

} // namespace DBus
} // namespace CommonAPI
