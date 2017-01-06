// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_CONFIG_HPP_
#define COMMONAPI_CONFIG_HPP_

#include <CommonAPI/Types.hpp>

namespace CommonAPI {

#ifndef DEFAULT_SEND_TIMEOUT
#define DEFAULT_SEND_TIMEOUT 5000
#endif

static const Timeout_t DEFAULT_SEND_TIMEOUT_MS = DEFAULT_SEND_TIMEOUT;

} // namespace CommonAPI

#endif // COMMONAPI_CONFIG_HPP_
