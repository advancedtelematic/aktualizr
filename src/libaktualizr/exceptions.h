#ifndef EXCEPTIONS_H_
#define EXCEPTIONS_H_

#include <stdexcept>
#include <string>
#include "logging/logging.h"

class FatalException : public std::logic_error {
 public:
  explicit FatalException(const std::string &what_arg) : std::logic_error(what_arg.c_str()) { LOG_FATAL << what_arg; }
  ~FatalException() noexcept override = default;
};

class NotImplementedException : public std::logic_error {
 public:
  NotImplementedException() : std::logic_error("Function not yet implemented.") {}
  ~NotImplementedException() noexcept override = default;
};

#endif
