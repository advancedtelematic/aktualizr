// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_LOGGER_IMPL_HPP_
#define COMMONAPI_LOGGER_IMPL_HPP_

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>

#include <CommonAPI/Export.hpp>


namespace CommonAPI {

class Logger;

class LoggerImpl {
public:
    friend class Logger;

    enum class Level : uint8_t COMMONAPI_EXPORT {
      LL_FATAL = 0, LL_ERROR = 1, LL_WARNING = 2, LL_INFO = 3, LL_DEBUG = 4, LL_VERBOSE = 5
    };

    static void init(bool, const std::string &, bool, const std::string &);

private:

  LoggerImpl();
  ~LoggerImpl();

  COMMONAPI_EXPORT static bool isLogged(Level _level);
    
  COMMONAPI_EXPORT static LoggerImpl* get();

  COMMONAPI_EXPORT void doLog(Level _level, const std::string &_message);

#if defined(USE_CONSOLE) || defined(USE_FILE) || defined(USE_DLT)
    static Level stringAsLevel(const std::string &_level);
#endif
#if defined(USE_CONSOLE) || defined(USE_FILE)
    static std::string levelAsString(Level _level);
#endif
#ifdef USE_DLT
    static DltLogLevelType levelAsDlt(Level _level);
#endif
#if defined(USE_CONSOLE) || defined(USE_FILE)
    static std::mutex mutex_;
#endif
#if defined(USE_CONSOLE) || defined(USE_FILE) || defined(USE_DLT)
    static Level maximumLogLevel_;
#endif
#ifdef USE_CONSOLE
    static bool useConsole_;
#endif
#ifdef USE_FILE
    static std::shared_ptr<std::ofstream> file_;
#endif
#ifdef USE_DLT
    static bool useDlt_;
    DLT_DECLARE_CONTEXT(dlt_);
    bool ownAppID_;
#endif
};

} // namespace CommonAPI

#endif // COMMONAPI_LOGGER_IMPL_HPP_
