// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_LOGGER_HPP_
#define COMMONAPI_LOGGER_HPP_

#include <CommonAPI/LoggerImpl.hpp>

#define COMMONAPI_LOGLEVEL_NONE       0
#define COMMONAPI_LOGLEVEL_FATAL      1
#define COMMONAPI_LOGLEVEL_ERROR      2
#define COMMONAPI_LOGLEVEL_WARNING    3
#define COMMONAPI_LOGLEVEL_INFO       4
#define COMMONAPI_LOGLEVEL_DEBUG      5
#define COMMONAPI_LOGLEVEL_VERBOSE    6

#ifndef COMMONAPI_LOGLEVEL
#define COMMONAPI_LOGLEVEL COMMONAPI_LOGLEVEL_NONE
#endif

#define COMMONAPI_FATAL     CommonAPI::Logger::fatal
#define COMMONAPI_ERROR     CommonAPI::Logger::error
#define COMMONAPI_WARNING   CommonAPI::Logger::warning
#define COMMONAPI_INFO      CommonAPI::Logger::info
#define COMMONAPI_DEBUG     CommonAPI::Logger::debug
#define COMMONAPI_VERBOSE   CommonAPI::Logger::verbose

namespace CommonAPI {

class Logger {
public:

    template<typename... LogEntries_>
    COMMONAPI_EXPORT static void fatal(LogEntries_... _entries) {
        log(LoggerImpl::Level::LL_FATAL, _entries...);
    }

    template<typename... LogEntries_>
    COMMONAPI_EXPORT static void error(LogEntries_... _entries) {
#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_ERROR
        log(LoggerImpl::Level::LL_ERROR, _entries...);
#else
    std::tuple<LogEntries_...> args(_entries...);
#endif
    }

    template<typename... LogEntries_>
    COMMONAPI_EXPORT static void warning(LogEntries_... _entries) {
#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_WARNING
        log(LoggerImpl::Level::LL_WARNING, _entries...);
#else
    std::tuple<LogEntries_...> args(_entries...);
#endif
    }

    template<typename... LogEntries_>
    COMMONAPI_EXPORT static void info(LogEntries_... _entries) {
#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_INFO
        log(LoggerImpl::Level::LL_INFO, _entries...);
#else
    std::tuple<LogEntries_...> args(_entries...);
#endif
    }

    template<typename... LogEntries_>
    COMMONAPI_EXPORT static void debug(LogEntries_... _entries) {
#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_DEBUG
        log(LoggerImpl::Level::LL_DEBUG, _entries...);
#else
    std::tuple<LogEntries_...> args(_entries...);
#endif
    }

    template<typename... LogEntries_>
    COMMONAPI_EXPORT static void verbose(LogEntries_... _entries) {
#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_VERBOSE
        log(LoggerImpl::Level::LL_VERBOSE, _entries...);
#else
    std::tuple<LogEntries_...> args(_entries...);
#endif
    }

    template<typename... LogEntries_>
    COMMONAPI_EXPORT static void log(LoggerImpl::Level _level, LogEntries_... _entries) {
#if defined(USE_CONSOLE) || defined(USE_FILE) || defined(USE_DLT)
      if (LoggerImpl::isLogged(_level)) {
            std::stringstream buffer;
            log_intern(buffer, _entries...);
            LoggerImpl::get()->doLog(_level, buffer.str());
        }
#else
        (void)_level;
        std::tuple<LogEntries_...> args(_entries...);
#endif
    }

    COMMONAPI_EXPORT static void init(bool, const std::string &, bool, const std::string &);

private:
    COMMONAPI_EXPORT static void log_intern(std::stringstream &_buffer) {
        (void)_buffer;
    }

    template<typename LogEntry_, typename... MoreLogEntries_>
    COMMONAPI_EXPORT static void log_intern(std::stringstream &_buffer, LogEntry_ _entry, MoreLogEntries_... _moreEntries) {
        _buffer << _entry;
        log_intern(_buffer, _moreEntries...);
    }
};

} // namespace CommonAPI

#endif // COMMONAPI_LOGGER_HPP_
