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

class TargetHashMismatch : public Exception {
 public:
  TargetHashMismatch(const std::string targetname)
      : Exception(targetname, "The target's calculated hash did not match the hash in the metadata.") {}
  virtual ~TargetHashMismatch() throw() {}
};

class OversizedTarget : public Exception {
 public:
  OversizedTarget(const std::string reponame)
      : Exception(reponame, "The target's size was greater than the size in the metadata.") {}
  virtual ~OversizedTarget() throw() {}
};

class IllegalThreshold : public Exception {
 public:
  IllegalThreshold(const std::string reponame, const std::string &what_arg) : Exception(reponame, what_arg.c_str()) {}
  virtual ~IllegalThreshold() throw() {}
};

class MissingRepo : public Exception {
 public:
  MissingRepo(const std::string reponame) : Exception(reponame, "The " + reponame + " repo is missing.") {}
  virtual ~MissingRepo() throw() {}
};

class UnmetThreshold : public Exception {
 public:
  UnmetThreshold(const std::string reponame, const std::string &role)
      : Exception(reponame, "The " + role + " metadata had an unmet threshold.") {}
  virtual ~UnmetThreshold() throw() {}
};

class ExpiredMetadata : public Exception {
 public:
  ExpiredMetadata(const std::string reponame, const std::string &role)
      : Exception(reponame, "The " + role + " metadata was expired.") {}
  virtual ~ExpiredMetadata() throw() {}
};

class InvalidMetadata : public Exception {
 public:
  InvalidMetadata(const std::string reponame, const std::string &role, const std::string reason)
      : Exception(reponame, "The " + role + " metadata failed to parse:" + reason) {}
  virtual ~InvalidMetadata() throw() {}
};

class IllegalRsaKeySize : public Exception {
 public:
  IllegalRsaKeySize(const std::string reponame) : Exception(reponame, "The RSA key had an illegal size.") {}
  virtual ~IllegalRsaKeySize() throw() {}
};

class MissMatchTarget : public Exception {
 public:
  MissMatchTarget(const std::string reponame)
      : Exception(reponame, "The target missmatch between image and director.") {}
  virtual ~MissMatchTarget() throw() {}
};

class NonUniqueSignatures : public Exception {
 public:
  NonUniqueSignatures(const std::string reponame, const std::string &role)
      : Exception(reponame, "The role " + role + " had non-unique signatures.") {}
  virtual ~NonUniqueSignatures() throw() {}
};
}

#endif
