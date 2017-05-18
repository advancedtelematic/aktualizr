#ifndef UPTANE_EXCEPTIONS_H_
#define UPTANE_EXCEPTIONS_H_

#include <stdexcept>
#include <string>

namespace Uptane {

class Exception : public std::logic_error {
 public:
  Exception(const std::string reponame, const std::string &what_arg)
      : std::logic_error(what_arg.c_str()), reponame_(reponame) {}
  virtual ~Exception() throw() {}
  virtual std::string getName() { return reponame_; };

 protected:
  std::string reponame_;
};

class SecurityException : public Exception {
 public:
  SecurityException(const std::string reponame, const std::string &what_arg) : Exception(reponame, what_arg.c_str()) {}
  virtual ~SecurityException() throw() {}

};

static const std::string HASH_METADATA_MISMATCH =
    "The target's calculated hash did not match the hash in the metadata.";
class TargetHashMismatch : public Exception {
 public:
  TargetHashMismatch(const std::string reponame, const std::string &what_arg) : Exception(reponame, what_arg.c_str()) {}
  virtual ~TargetHashMismatch() throw() {}
  const std::string reponame_;
};

class OversizedTarget : public Exception {
 public:
  OversizedTarget(const std::string reponame)
      : Exception(reponame, "The target's size was greater than the size in the metadata.") {}
  virtual ~OversizedTarget() throw() {}
  const std::string reponame_;
};

class IllegalThreshold : public Exception {
 public:
  IllegalThreshold(const std::string reponame, const std::string &what_arg) : Exception(reponame, what_arg.c_str()) {}
  virtual ~IllegalThreshold() throw() {}
  const std::string reponame_;
};
};

#endif