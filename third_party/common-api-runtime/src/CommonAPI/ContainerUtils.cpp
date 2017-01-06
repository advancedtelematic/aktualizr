// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/ContainerUtils.hpp>
#include <CommonAPI/Types.hpp>

namespace CommonAPI {

size_t SharedPointerClientIdContentHash::operator()(const std::shared_ptr<ClientId>& t) const {
    if (t) {
        return t->hashCode();
    } else {
        return 0;
    }
}

bool SharedPointerClientIdContentEqual::operator()(const std::shared_ptr<ClientId>& a, const std::shared_ptr<ClientId>& b) const {
    if (a && b) {
        return *a==*b;
    } else {
        return false;
    }
}

} // namespace std
