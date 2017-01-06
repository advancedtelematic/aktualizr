// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_CONTAINERUTILS_HPP_
#define COMMONAPI_CONTAINERUTILS_HPP_

#include <functional>
#include <memory>

#include <CommonAPI/Export.hpp>

namespace CommonAPI {
class ClientId;

struct COMMONAPI_EXPORT SharedPointerClientIdContentHash : public std::unary_function<std::shared_ptr<ClientId>, size_t> {
    size_t operator()(const std::shared_ptr<ClientId>& t) const;
};

struct COMMONAPI_EXPORT SharedPointerClientIdContentEqual : public std::binary_function<std::shared_ptr<ClientId>, std::shared_ptr<ClientId>, bool> {
    bool operator()(const std::shared_ptr<ClientId>& a, const std::shared_ptr<ClientId>& b) const;
};


}  // namespace std


#endif // COMMONAPI_CONTAINERUTILS_HPP_
