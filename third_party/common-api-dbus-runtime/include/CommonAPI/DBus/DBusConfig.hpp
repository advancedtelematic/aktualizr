// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSCONFIG_HPP_
#define COMMONAPI_DBUS_DBUSCONFIG_HPP_

#include <CommonAPI/CallInfo.hpp>

namespace CommonAPI {
namespace DBus {

static const Timeout_t DEFAULT_SEND_TIMEOUT_MS = 5000;
static CommonAPI::CallInfo defaultCallInfo(DEFAULT_SEND_TIMEOUT_MS);

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSCONFIG_HPP_
