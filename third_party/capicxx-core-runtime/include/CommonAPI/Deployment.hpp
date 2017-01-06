// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DEPLOYMENT_HPP_
#define COMMONAPI_DEPLOYMENT_HPP_

#include <tuple>

namespace CommonAPI {
// The binding-specific deployment parameters should be
// defined like this:
//
// struct BindingUInt16Deployment : CommonAPI::Deployment<> {
//         // Binding-specific bool deployment parameters
// };
//
// struct BindingStringDeployment : CommonAPI::Deployment<> {
//         // Binding-specific String deployment parameters
// };
//
// template<typename... Types_>
// struct BindingStructDeployment
//             : CommonAPI::Deployment<Types_...> {
//         BindingStructDeployment(<SPECIFIC PARAMETERS>, Types_... t)
//             : CommonAPI::Deployment<Types_...>(t),
//               <SPECIFIC INITIALIZERS> {};
//
//         // Binding-specific struct deployment parameters
// };
//
// The generated code needs to use these definitions to
// provide the deployment informations for the actual data.
// E.g., for struct consisting of a boolean and a string
// value, it needs to generate:
//
// CommonAPI::BindingStructDeployment<
//     CommonAPI::BindingBoolDeployment,
//     CommonAPI::BindingStringDeployment
//  > itsDeployment(<PARAMS);

struct EmptyDeployment {};

template<typename ElementDepl_>
struct ArrayDeployment {
    ArrayDeployment(ElementDepl_ *_elementDepl)
        : elementDepl_(_elementDepl) {}

    ElementDepl_ *elementDepl_;
};

template<typename KeyDepl_, typename ValueDepl_>
struct MapDeployment {
    MapDeployment(KeyDepl_ *_key, ValueDepl_ *_value)
        : key_(_key), value_(_value) {}

    const KeyDepl_ *key_;
    const ValueDepl_ *value_;
};

// The following shall be used as a base for structure/variant deployments.
template<typename... Types_>
struct Deployment {
    Deployment(Types_*... _values) : values_(_values...) {}
    std::tuple<Types_*...> values_;
};

} // namespace CommonAPI

#endif // COMMONAPI_DEPLOYABLE_HPP_
