// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSUTILS_HPP_
#define COMMONAPI_DBUS_DBUSUTILS_HPP_

#include <future>

namespace CommonAPI {
namespace DBus {

//In gcc 4.4.1, the enumeration "std::future_status" is defined, but the return values of some functions
//are bool where the same functions in gcc 4.6. return a value from this enum. This template is a way
//to ensure compatibility for this issue.
template<typename FutureWaitType_>
inline bool checkReady(FutureWaitType_&);

template<>
inline bool checkReady<bool>(bool& returnedValue) {
    return returnedValue;
}

template<>
inline bool checkReady<std::future_status>(std::future_status& returnedValue) {
    return returnedValue == std::future_status::ready;
}

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSUTILS_HPP_
