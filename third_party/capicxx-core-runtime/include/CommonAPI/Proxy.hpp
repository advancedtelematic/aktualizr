// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_PROXY_HPP_
#define COMMONAPI_PROXY_HPP_

#include <cstdint>
#include <memory>
#include <type_traits>

#include <CommonAPI/Address.hpp>
#include <CommonAPI/Attribute.hpp>
#include <CommonAPI/Export.hpp>
#include <CommonAPI/Version.hpp>

namespace CommonAPI {

typedef Event<AvailabilityStatus> ProxyStatusEvent;
typedef ReadonlyAttribute<Version> InterfaceVersionAttribute;

class Proxy {
public:
    COMMONAPI_EXPORT virtual ~Proxy() {}

    COMMONAPI_EXPORT const Address &getAddress() const;

    COMMONAPI_EXPORT virtual bool isAvailable() const = 0;

    COMMONAPI_EXPORT virtual bool isAvailableBlocking() const = 0;

    COMMONAPI_EXPORT virtual ProxyStatusEvent& getProxyStatusEvent() = 0;

    COMMONAPI_EXPORT virtual InterfaceVersionAttribute& getInterfaceVersionAttribute() = 0;

protected:
    Address address_;
};

} // namespace CommonAPI

#endif // COMMONAPI_PROXY_HPP_
