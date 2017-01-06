// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Runtime.hpp>

namespace CommonAPI {;
void
  Logger::init(bool _useConsole, const std::string &_fileName, bool _useDlt, const std::string &_level) {
  LoggerImpl::init(_useConsole, _fileName, _useDlt, _level);
}

} //namespace CommonAPI
