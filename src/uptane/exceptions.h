#ifndef UPTANE_EXCEPTIONS_H_
#define UPTANE_EXCEPTIONS_H_

#include <stdexcept>
#include <string>

namespace Uptane {

class Exception : public std::logic_error {
 public:
  Exception(const std::string reponame, const std::string &what_arg)
      : std::logic_error(what_arg.c_str()), reponame_(reponame) {}
  ~Exception() throw() override {}
  virtual std::string getName() { return reponame_; };

 protected:
  std::string reponame_;
};

class SecurityException : public Exception {
 public:
  SecurityException(const std::string reponame, const std::string &what_arg) : Exception(reponame, what_arg.c_str()) {}
  ~SecurityException() throw() override {}
};

class TargetHashMismatch : public Exception {
 public:
  TargetHashMismatch(const std::string targetname)
      : Exception(targetname, "The target's calculated hash did not match the hash in the metadata.") {}
  ~TargetHashMismatch() throw() override {}
};

class OversizedTarget : public Exception {
 public:
  OversizedTarget(const std::string reponame)
      : Exception(reponame, "The target's size was greater than the size in the metadata.") {}
  ~OversizedTarget() throw() override {}
};

class IllegalThreshold : public Exception {
 public:
  IllegalThreshold(const std::string reponame, const std::string &what_arg) : Exception(reponame, what_arg.c_str()) {}
  ~IllegalThreshold() throw() override {}
};

class MissingRepo : public Exception {
 public:
  MissingRepo(const std::string reponame) : Exception(reponame, "The " + reponame + " repo is missing.") {}
  ~MissingRepo() throw() override {}
};

class UnmetThreshold : public Exception {
 public:
  UnmetThreshold(const std::string reponame, const std::string &role)
      : Exception(reponame, "The " + role + " metadata had an unmet threshold.") {}
  ~UnmetThreshold() throw() override {}
};

class ExpiredMetadata : public Exception {
 public:
  ExpiredMetadata(const std::string reponame, const std::string &role)
      : Exception(reponame, "The " + role + " metadata was expired.") {}
  ~ExpiredMetadata() throw() override {}
};

class InvalidMetadata : public Exception {
 public:
  InvalidMetadata(const std::string reponame, const std::string &role, const std::string reason)
      : Exception(reponame, "The " + role + " metadata failed to parse:" + reason) {}
  ~InvalidMetadata() throw() override {}
};

class IllegalRsaKeySize : public Exception {
 public:
  IllegalRsaKeySize(const std::string reponame) : Exception(reponame, "The RSA key had an illegal size.") {}
  ~IllegalRsaKeySize() throw() override {}
};

class MissMatchTarget : public Exception {
 public:
  MissMatchTarget(const std::string reponame)
      : Exception(reponame, "The target missmatch between image and director.") {}
  ~MissMatchTarget() throw() override {}
};

class NonUniqueSignatures : public Exception {
 public:
  NonUniqueSignatures(const std::string reponame, const std::string &role)
      : Exception(reponame, "The role " + role + " had non-unique signatures.") {}
  ~NonUniqueSignatures() throw() override {}
};
}  // namespace Uptane

#endif
