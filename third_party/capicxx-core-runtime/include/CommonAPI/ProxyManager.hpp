// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_PROXY_MANAGER_HPP_
#define COMMONAPI_PROXY_MANAGER_HPP_

#include <functional>
#include <future>
#include <string>
#include <vector>

#include <CommonAPI/Event.hpp>
#include <CommonAPI/Proxy.hpp>
#include <CommonAPI/Types.hpp>
#include <CommonAPI/Runtime.hpp>

namespace CommonAPI {

class ProxyManager {
public:
    typedef std::function<void(const CallStatus &, const std::vector<std::string> &)> GetAvailableInstancesCallback;
    typedef std::function<void(const CallStatus &, const AvailabilityStatus &)> GetInstanceAvailabilityStatusCallback;

    typedef Event<std::string, AvailabilityStatus> InstanceAvailabilityStatusChangedEvent;

    ProxyManager() = default;
    ProxyManager(ProxyManager &&) = delete;
    ProxyManager(const ProxyManager &) = delete;

    virtual ~ProxyManager() {}

    virtual const std::string &getDomain() const = 0;
    virtual const std::string &getInterface() const = 0;
    virtual const ConnectionId_t &getConnectionId() const = 0;

    virtual void getAvailableInstances(CommonAPI::CallStatus&, std::vector<std::string>& availableInstances) = 0;
    virtual std::future<CallStatus> getAvailableInstancesAsync(GetAvailableInstancesCallback callback) = 0;

    virtual void getInstanceAvailabilityStatus(const std::string& instanceAddress,
                                               CallStatus& callStatus,
                                               AvailabilityStatus& availabilityStatus) = 0;

    virtual std::future<CallStatus> getInstanceAvailabilityStatusAsync(const std::string&,
                                                                       GetInstanceAvailabilityStatusCallback callback) = 0;

    virtual InstanceAvailabilityStatusChangedEvent& getInstanceAvailabilityStatusChangedEvent() = 0;

    template<template<typename ...> class ProxyClass_, typename ... AttributeExtensions_>
    std::shared_ptr<ProxyClass_<AttributeExtensions_...> >
    buildProxy(const std::string &_instance, const ConnectionId_t& _connectionId = DEFAULT_CONNECTION_ID) {
        std::shared_ptr<Proxy> proxy = createProxy(getDomain(), getInterface(), _instance, (DEFAULT_CONNECTION_ID == _connectionId) ? getConnectionId() : _connectionId);
        if (proxy) {
            return std::make_shared<ProxyClass_<AttributeExtensions_...>>(proxy);
        }
        return NULL;
    }

    template<template<typename ...> class ProxyClass_, typename ... AttributeExtensions_>
    std::shared_ptr<ProxyClass_<AttributeExtensions_...> >
    buildProxy(const std::string &_instance, std::shared_ptr<MainLoopContext> _context) {
        std::shared_ptr<Proxy> proxy = createProxy(getDomain(), getInterface(), _instance, _context);
        if (proxy) {
            return std::make_shared<ProxyClass_<AttributeExtensions_...>>(proxy);
        }
        return NULL;
    }

protected:
    COMMONAPI_EXPORT std::shared_ptr<Proxy> createProxy(const std::string &,
                                       const std::string &,
                                       const std::string &,
                                       const ConnectionId_t &_connection) const;

    COMMONAPI_EXPORT std::shared_ptr<Proxy> createProxy(const std::string &,
                                       const std::string &,
                                       const std::string &,
                                       std::shared_ptr<MainLoopContext> _context) const;
};

} // namespace CommonAPI

#endif // COMMONAPI_PROXY_MANAGER_HPP_
