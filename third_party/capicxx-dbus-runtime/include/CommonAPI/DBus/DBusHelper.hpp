// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUS_HELPER_HPP_
#define COMMONAPI_DBUS_DBUS_HELPER_HPP_

#include <unordered_map>
#include <vector>

namespace CommonAPI {
namespace DBus {

template <int ...>
struct index_sequence {};


template <int N_, int ...S_>
struct make_sequence : make_sequence<N_-1, N_-1, S_...> {};

template <int ...S_>
struct make_sequence<0, S_...> {
    typedef index_sequence<S_...> type;
};


template <int N_, int Offset_, int ...S_>
struct make_sequence_range : make_sequence_range<N_-1, Offset_, N_-1+Offset_, S_...> {};

template <int Offset_, int ...S_>
struct make_sequence_range<0, Offset_, S_...> {
    typedef index_sequence<S_...> type;
};

template<typename Type>
struct is_std_vector { static const bool value = false; };

template<typename Type_>
struct is_std_vector<std::vector<Type_> > { static const bool value = true; };

template<typename Type_>
struct is_std_unordered_map { static const bool value = false; };

template<typename KeyType_, typename ValueType_, typename HasherType_>
struct is_std_unordered_map<std::unordered_map<KeyType_, ValueType_, HasherType_>> { static const bool value = true; };

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUS_HELPER_HPP_
