// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_LOGGER_HPP_
#define COMMONAPI_LOGGER_HPP_

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>

#include <CommonAPI/Export.hpp>

#define COMMONAPI_LOGLEVEL_FATAL      0
#define COMMONAPI_LOGLEVEL_ERROR      1
#define COMMONAPI_LOGLEVEL_WARNING    2
#define COMMONAPI_LOGLEVEL_INFO       3
#define COMMONAPI_LOGLEVEL_DEBUG      4
#define COMMONAPI_LOGLEVEL_VERBOSE    5

#ifndef COMMONAPI_LOGLEVEL
#define COMMONAPI_LOGLEVEL COMMONAPI_LOGLEVEL_INFO
#endif

#ifdef WIN32

#define COMMONAPI_FATAL(...) \
    do { Logger::log(Logger::Level::LL_FATAL, __VA_ARGS__); } while (false);

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_ERROR
#define COMMONAPI_ERROR(...) \
        do { Logger::log(Logger::Level::LL_ERROR, __VA_ARGS__); } while (false);
#else
#define COMMONAPI_ERROR(...)
#endif

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_WARNING
#define COMMONAPI_WARNING(...) \
        do { Logger::log(Logger::Level::LL_WARNING, __VA_ARGS__); } while (false);
#else
#define COMMONAPI_WARNING(...)
#endif

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_INFO
#define COMMONAPI_INFO(...) \
        do { Logger::log(Logger::Level::LL_INFO, __VA_ARGS__); } while (false);
#else
#define COMMONAPI_INFO(...)
#endif

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_DEBUG
#define COMMONAPI_DEBUG(...) \
        do { Logger::log(Logger::Level::LL_DEBUG, __VA_ARGS__); } while (false);
#else
#define COMMONAPI_DEBUG(...)
#endif

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_VERBOSE
#define COMMONAPI_VERBOSE(...) \
        do { Logger::log(Logger::Level::LL_VERBOSE, __VA_ARGS__); } while (false);
#else
#define COMMONAPI_VERBOSE(...)
#endif

#else // !WIN32

#define COMMONAPI_FATAL(params...) \
    do { Logger::log(Logger::Level::LL_FATAL, params); } while (false);

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_ERROR
    #define COMMONAPI_ERROR(params...) \
        do { Logger::log(Logger::Level::LL_ERROR, params); } while (false);
#else
    #define COMMONAPI_ERROR(params...)
#endif

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_WARNING
    #define COMMONAPI_WARNING(params...) \
        do { Logger::log(Logger::Level::LL_WARNING, params); } while (false);
#else
    #define COMMONAPI_WARNING(params...)
#endif

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_INFO
    #define COMMONAPI_INFO(params...) \
        do { Logger::log(Logger::Level::LL_INFO, params); } while (false);
#else
    #define COMMONAPI_INFO(params...)
#endif

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_DEBUG
    #define COMMONAPI_DEBUG(params...) \
        do { Logger::log(Logger::Level::LL_DEBUG, params); } while (false);
#else
    #define COMMONAPI_DEBUG(params...)
#endif

#if COMMONAPI_LOGLEVEL >= COMMONAPI_LOGLEVEL_VERBOSE
    #define COMMONAPI_VERBOSE(params...) \
        do { Logger::log(Logger::Level::LL_VERBOSE, params); } while (false);
#else
    #define COMMONAPI_VERBOSE(params...)
#endif

#endif // WIN32

namespace CommonAPI {

class Logger {
public:
    COMMONAPI_EXPORT enum class Level : uint8_t {
        LL_FATAL = 0, LL_ERROR = 1, LL_WARNING = 2, LL_INFO = 3, LL_DEBUG = 4, LL_VERBOSE = 5
    };

    COMMONAPI_EXPORT Logger();
    COMMONAPI_EXPORT ~Logger();

    template<typename... LogEntries_>
    COMMONAPI_EXPORT static void log(Level _level, LogEntries_... _entries) {
#if defined(USE_CONSOLE) || defined(USE_FILE) || defined(USE_DLT)
        if (_level < maximumLogLevel_) {
            std::stringstream buffer;
            log_intern(buffer, _entries...);
            Logger::get()->doLog(_level, buffer.str());
        }
#else
        (void)_level;
        std::tuple<LogEntries_...> args(_entries...);
#endif
    }

    COMMONAPI_EXPORT static void init(bool, const std::string &, bool, const std::string &);

private:
    COMMONAPI_EXPORT static inline Logger *get() {
        static Logger theLogger;
        return &theLogger;
    }

    COMMONAPI_EXPORT static void log_intern(std::stringstream &_buffer) {
        (void)_buffer;
    }

    template<typename LogEntry_, typename... MoreLogEntries_>
    COMMONAPI_EXPORT static void log_intern(std::stringstream &_buffer, LogEntry_ _entry, MoreLogEntries_... _moreEntries) {
        _buffer << _entry;
        log_intern(_buffer, _moreEntries...);
    }

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
#endif
};

} // namespace CommonAPI

#endif // COMMONAPI_LOGGER_HPP_
