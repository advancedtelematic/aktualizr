// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUS_FUNCTIONAL_HASH_HPP_
#define COMMONAPI_DBUS_DBUS_FUNCTIONAL_HASH_HPP_

#include <functional>
#include <string>
#include <tuple>

#include <CommonAPI/Export.hpp>

namespace std {

template<>
struct COMMONAPI_EXPORT hash<pair<const char*, const char*> >
    : public unary_function<pair<const char*, const char*>, size_t> {

    size_t operator()(const pair<const char*, const char*>& t) const;
};

template<>
struct COMMONAPI_EXPORT hash<const char*>
    : public unary_function<const char*, size_t> {

    size_t operator()(const char* const t) const;
};

template<>
struct COMMONAPI_EXPORT hash<pair<string, string> >
    : public unary_function<pair<string, string>, size_t> {

    size_t operator()(const pair<string, string>& t) const;
};

template<>
struct COMMONAPI_EXPORT hash<tuple<string, string, string> >
    : public unary_function<tuple<string, string, string>, size_t> {

    size_t operator()(const tuple<string, string, string>& t) const;
};

template<>
struct COMMONAPI_EXPORT hash<tuple<string, string, string, bool> >
    : public unary_function<tuple<string, string, string, bool>, size_t> {

    size_t operator()(const tuple<string, string, string, bool>& t) const;
};

template<>
struct COMMONAPI_EXPORT hash<tuple<string, string, string, int> >
    : public unary_function<tuple<string, string, string, int>, size_t> {

    size_t operator()(const tuple<string, string, string, int>& t) const;
};

template<>
struct COMMONAPI_EXPORT hash<tuple<string, string, string, string> >
    : public std::unary_function<tuple<string, string, string, string>, size_t> {

    size_t operator()(const tuple<string, string, string, string>& t) const;
};

template<>
struct COMMONAPI_EXPORT equal_to<pair<const char*, const char*> >
    : public binary_function<pair<const char*, const char*>,
      pair<const char*, const char*>,
      bool> {

    bool operator()(const pair<const char*, const char*>& a, const pair<const char*, const char*>& b) const;
};

} // namespace std

#endif // COMMONAPI_DBUS_DBUS_FUNCTIONAL_HASH_HPP_
