// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSDEPLOYMENTS_HPP_
#define COMMONAPI_DBUS_DBUSDEPLOYMENTS_HPP_

#include <string>
#include <unordered_map>

#include <CommonAPI/Deployment.hpp>
#include <CommonAPI/Export.hpp>

namespace CommonAPI {
namespace DBus {

template<typename... Types_>
struct VariantDeployment : CommonAPI::Deployment<Types_...> {
    static const size_t size_ = std::tuple_size<std::tuple<Types_...>>::value;
    VariantDeployment(bool _isDBus, Types_*... _t)
          : CommonAPI::Deployment<Types_...>(_t...),
            isDBus_(_isDBus) {};
    bool isDBus_;
};

struct StringDeployment : CommonAPI::Deployment<> {
    StringDeployment(bool _isObjectPath)
    : isObjectPath_(_isObjectPath) {};

    bool isObjectPath_;
};

template<typename... Types_>
struct StructDeployment : CommonAPI::Deployment<Types_...> {
    StructDeployment(Types_*... t)
    : CommonAPI::Deployment<Types_...>(t...) {};
};

template<typename ElementDepl_>
struct ArrayDeployment : CommonAPI::ArrayDeployment<ElementDepl_> {
    ArrayDeployment(ElementDepl_ *_element)
    : CommonAPI::ArrayDeployment<ElementDepl_>(_element) {}
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSDEPLOYMENTS_HPP_
