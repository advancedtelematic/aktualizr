// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>

#include <CommonAPI/LoggerImpl.hpp>
#ifdef USE_DLT
#include <CommonAPI/Runtime.hpp>
#endif

namespace CommonAPI {;

#if defined(USE_CONSOLE) || defined(USE_FILE)
std::mutex LoggerImpl::mutex_;
#endif

#ifdef USE_CONSOLE
bool LoggerImpl::useConsole_(true);
#endif

#ifdef USE_FILE
std::shared_ptr<std::ofstream> LoggerImpl::file_;
#endif

#ifdef USE_DLT
bool LoggerImpl::useDlt_(false);
#endif

#if defined(USE_CONSOLE) || defined(USE_FILE) || defined(USE_DLT)
LoggerImpl::Level LoggerImpl::maximumLogLevel_(LoggerImpl::Level::LL_INFO);
#endif

LoggerImpl::LoggerImpl() {
#ifdef USE_DLT
  if (useDlt_) {
    std::string app = Runtime::getProperty("LogApplication");
    if (!app.empty()) {
      ownAppID_ = true;
      DLT_REGISTER_APP(app.c_str(), "CommonAPI Application");
    }

    std::string context = Runtime::getProperty("LogContext");
    if (context == "") context = "CAPI";
    DLT_REGISTER_CONTEXT(dlt_, context.c_str(), "CAPI");
  }
#endif
}

LoggerImpl::~LoggerImpl() {
#ifdef USE_DLT
  if (useDlt_) {
    DLT_UNREGISTER_CONTEXT(dlt_);
    if (ownAppID_) {
      DLT_UNREGISTER_APP();
    }
  }
#endif
}

LoggerImpl* LoggerImpl::get() {
  static LoggerImpl theLoggerImpl;
  return &theLoggerImpl;
}


void
LoggerImpl::init(bool _useConsole, const std::string &_fileName, bool _useDlt, const std::string &_level) {
#ifdef USE_CONSOLE
  useConsole_ = _useConsole;
#else
  (void)_useConsole;
#endif
#ifdef USE_FILE
  if (_fileName != "") {
    file_ = std::make_shared<std::ofstream>();
    if (file_)
      file_->open(_fileName.c_str(), std::ofstream::out | std::ofstream::app);
  }
#else
  (void)_fileName;
#endif
#ifdef USE_DLT
  useDlt_ = _useDlt;
#else
  (void)_useDlt;
#endif
#if defined(USE_CONSOLE) || defined(USE_FILE) || defined(USE_DLT)
  maximumLogLevel_ = stringAsLevel(_level);
#else
  (void)_level;
#endif
}

void
LoggerImpl::doLog(Level _level, const std::string &_message) {
#ifdef USE_CONSOLE
  if (useConsole_) {
    std::lock_guard<std::mutex> itsLock(mutex_);
    std::cout << "[CAPI][" << levelAsString(_level) << "] " << _message << std::endl;
  }
#endif
#ifdef USE_FILE
  if (file_ && file_->is_open()) {
    std::lock_guard<std::mutex> itsLock(mutex_);
    (*(file_.get())) << "[CAPI][" << levelAsString(_level) << "] " << _message << std::endl;
  }
#endif
#ifdef USE_DLT
  if (useDlt_) {
    DLT_LOG_STRING(dlt_, levelAsDlt(_level), _message.c_str());
  }
#endif
#if !defined(USE_CONSOLE) && !defined(USE_FILE) && !defined(USE_DLT)
  (void)_level;
  (void)_message;
#endif
}

#if defined(USE_CONSOLE) || defined(USE_FILE) || defined(USE_DLT)
LoggerImpl::Level
LoggerImpl::stringAsLevel(const std::string &_level) {
  if (_level == "fatal")
    return Level::LL_FATAL;

  if (_level == "error")
    return Level::LL_ERROR;

  if (_level == "warning")
    return Level::LL_WARNING;

  if (_level == "info")
    return Level::LL_INFO;

  if (_level == "debug")
    return Level::LL_DEBUG;

  if (_level == "verbose")
    return Level::LL_VERBOSE;

  return Level::LL_INFO;
}
#endif

#if defined(USE_CONSOLE) || defined(USE_FILE)
std::string
LoggerImpl::levelAsString(LoggerImpl::Level _level) {
  switch (_level) {
  case Level::LL_FATAL:
    return "FATAL";
  case Level::LL_ERROR:
    return "ERROR";
  case Level::LL_WARNING:
    return "WARNING";
  case Level::LL_INFO:
    return "INFO";
  case Level::LL_DEBUG:
    return "DEBUG";
  case Level::LL_VERBOSE:
    return "VERBOSE";
  default:
    return "";
  }
}
#endif

#ifdef USE_DLT
DltLogLevelType
LoggerImpl::levelAsDlt(LoggerImpl::Level _level) {
  switch (_level) {
  case Level::LL_FATAL:
    return DLT_LOG_FATAL;
  case Level::LL_ERROR:
    return DLT_LOG_ERROR;
  case Level::LL_WARNING:
    return DLT_LOG_WARN;
  case Level::LL_INFO:
    return DLT_LOG_INFO;
  case Level::LL_DEBUG:
    return DLT_LOG_DEBUG;
  case Level::LL_VERBOSE:
    return DLT_LOG_VERBOSE;
  default:
    return DLT_LOG_DEFAULT;
  }
}
#endif

bool 
LoggerImpl::isLogged(LoggerImpl::Level _level) {
#if defined(USE_CONSOLE) || defined(USE_FILE) || defined(USE_DLT)
  return (_level <= maximumLogLevel_) ? true : false;
#else
  (void)_level;
  return false;
#endif
}

} // namespace CommonAPI
