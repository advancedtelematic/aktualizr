#ifndef UPTANE_EXCEPTIONS_H_
#define UPTANE_EXCEPTIONS_H_

#include <stdexcept>
#include <string>
#include <utility>

namespace Uptane {

class Exception : public std::logic_error {
 public:
  Exception(std::string reponame, const std::string& what_arg)
      : std::logic_error(what_arg.c_str()), reponame_(std::move(reponame)) {}
  ~Exception() noexcept override = default;
  virtual std::string getName() const { return reponame_; };

 protected:
  std::string reponame_;
};

class SecurityException : public Exception {
 public:
  SecurityException(const std::string& reponame, const std::string& what_arg) : Exception(reponame, what_arg) {}
  ~SecurityException() noexcept override = default;
};

class TargetHashMismatch : public Exception {
 public:
  explicit TargetHashMismatch(const std::string& targetname)
      : Exception(targetname, "The target's calculated hash did not match the hash in the metadata.") {}
  ~TargetHashMismatch() noexcept override = default;
};

class OversizedTarget : public Exception {
 public:
  explicit OversizedTarget(const std::string& reponame)
      : Exception(reponame, "The target's size was greater than the size in the metadata.") {}
  ~OversizedTarget() noexcept override = default;
};

class IllegalThreshold : public Exception {
 public:
  IllegalThreshold(const std::string& reponame, const std::string& what_arg) : Exception(reponame, what_arg) {}
  ~IllegalThreshold() noexcept override = default;
};

class MissingRepo : public Exception {
 public:
  explicit MissingRepo(const std::string& reponame) : Exception(reponame, "The " + reponame + " repo is missing.") {}
  ~MissingRepo() noexcept override = default;
};

class UnmetThreshold : public Exception {
 public:
  UnmetThreshold(const std::string& reponame, const std::string& role)
      : Exception(reponame, "The " + role + " metadata had an unmet threshold.") {}
  ~UnmetThreshold() noexcept override = default;
};

class ExpiredMetadata : public Exception {
 public:
  ExpiredMetadata(const std::string& reponame, const std::string& role)
      : Exception(reponame, "The " + role + " metadata was expired.") {}
  ~ExpiredMetadata() noexcept override = default;
};

class InvalidMetadata : public Exception {
 public:
  InvalidMetadata(const std::string& reponame, const std::string& role, const std::string& reason)
      : Exception(reponame, "The " + role + " metadata failed to parse:" + reason) {}
  ~InvalidMetadata() noexcept override = default;
};

class IllegalRsaKeySize : public Exception {
 public:
  explicit IllegalRsaKeySize(const std::string& reponame) : Exception(reponame, "The RSA key had an illegal size.") {}
  ~IllegalRsaKeySize() noexcept override = default;
};

class MissMatchTarget : public Exception {
 public:
  explicit MissMatchTarget(const std::string& reponame)
      : Exception(reponame, "The target missmatch between image and director.") {}
  ~MissMatchTarget() noexcept override = default;
};

class NonUniqueSignatures : public Exception {
 public:
  NonUniqueSignatures(const std::string& reponame, const std::string& role)
      : Exception(reponame, "The role " + role + " had non-unique signatures.") {}
  ~NonUniqueSignatures() noexcept override = default;
};
}  // namespace Uptane

#endif
