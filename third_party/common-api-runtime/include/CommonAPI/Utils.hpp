// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_UTILS_HPP_
#define COMMONAPI_UTILS_HPP_

#include <string>
#include <vector>

#include <CommonAPI/Export.hpp>

namespace CommonAPI {

std::vector<std::string> COMMONAPI_EXPORT split(const std::string& s, char delim);
void COMMONAPI_EXPORT trim(std::string &_s);


} //namespace CommonAPI

#endif /* COMMONAPI_UTILS_HPP_ */
