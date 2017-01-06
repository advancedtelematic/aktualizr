// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/ProxyManager.hpp>
#include <CommonAPI/Runtime.hpp>

namespace CommonAPI {

std::shared_ptr<Proxy>
ProxyManager::createProxy(
        const std::string &_domain, const std::string &_interface, const std::string &_instance,
        const ConnectionId_t &_connection) const {
    return Runtime::get()->createProxy(_domain, _interface, _instance, _connection);
}

} // namespace CommonAPI
