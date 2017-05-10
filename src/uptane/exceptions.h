#ifndef UPTANE_EXCEPTIONS_H_
#define UPTANE_EXCEPTIONS_H_

#include <stdexcept>
#include <string>

namespace Uptane {
class SecurityException : public std::logic_error {
 public:
  SecurityException(const std::string &what_arg) : std::logic_error(what_arg.c_str()) {}
};
};

#endif