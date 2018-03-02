#ifndef EXCEPTIONS_H_
#define EXCEPTIONS_H_

#include <stdexcept>
#include <string>

class FatalException : public std::logic_error {
 public:
  FatalException(const std::string &what_arg) : std::logic_error(what_arg.c_str()) { LOG_FATAL << what_arg; }
  ~FatalException() throw() override {}
};

class NotImplementedException : public std::logic_error {
 public:
  NotImplementedException() : std::logic_error("Function not yet implemented.") {}
  ~NotImplementedException() throw() override {}
};

#endif
