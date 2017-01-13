// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_VERSION_HPP_
#define COMMONAPI_VERSION_HPP_

#include <cstdint>

namespace CommonAPI {

struct Version {
    Version() = default;
    Version(const uint32_t &majorValue, const uint32_t &minorValue)
        : Major(majorValue), Minor(minorValue) {
    }

    uint32_t Major;
    uint32_t Minor;
};

} // namespace CommonAPI

#endif // COMMONAPI_STRUCT_HPP_
