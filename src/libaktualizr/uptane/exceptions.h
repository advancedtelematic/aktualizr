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

class MetadataFetchFailure : public Exception {
 public:
  MetadataFetchFailure(const std::string& reponame, const std::string& role)
      : Exception(reponame, std::string("Failed to fetch role ") + role + " in " + reponame + " repository.") {}
  ~MetadataFetchFailure() noexcept override = default;
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
      : Exception(reponame, "The " + role + " metadata failed to parse: " + reason) {}
  ~InvalidMetadata() noexcept override = default;
};

class TargetMismatch : public Exception {
 public:
  explicit TargetMismatch(const std::string& targetname)
      : Exception(targetname, "The target metadata in the Image and Director repos do not match.") {}
  ~TargetMismatch() noexcept override = default;
};

class NonUniqueSignatures : public Exception {
 public:
  NonUniqueSignatures(const std::string& reponame, const std::string& role)
      : Exception(reponame, "The role " + role + " had non-unique signatures.") {}
  ~NonUniqueSignatures() noexcept override = default;
};

class BadKeyId : public Exception {
 public:
  BadKeyId(const std::string& reponame) : Exception(reponame, "A key has an incorrect associated key ID") {}
  ~BadKeyId() noexcept override = default;
};

class BadEcuId : public Exception {
 public:
  BadEcuId(const std::string& reponame)
      : Exception(reponame, "The target had an ECU ID that did not match the client's configured ECU ID.") {}
  ~BadEcuId() noexcept override = default;
};

class BadHardwareId : public Exception {
 public:
  BadHardwareId(const std::string& reponame)
      : Exception(reponame, "The target had a hardware ID that did not match the client's configured hardware ID.") {}
  ~BadHardwareId() noexcept override = default;
};

class RootRotationError : public Exception {
 public:
  RootRotationError(const std::string& reponame)
      : Exception(reponame, "Version in Root metadata does not match its expected value.") {}
  ~RootRotationError() noexcept override = default;
};

class VersionMismatch : public Exception {
 public:
  VersionMismatch(const std::string& reponame, const std::string& role)
      : Exception(reponame, "The version of role " + role + " does not match the entry in Snapshot metadata.") {}
  ~VersionMismatch() noexcept override = default;
};

class DelegationHashMismatch : public Exception {
 public:
  explicit DelegationHashMismatch(const std::string& delegation_name)
      : Exception("image", "The calculated hash of delegated role " + delegation_name +
                               " did not match the hash in the metadata.") {}
  ~DelegationHashMismatch() noexcept override = default;
};

class DelegationMissing : public Exception {
 public:
  explicit DelegationMissing(const std::string& delegation_name)
      : Exception("image", "The delegated role " + delegation_name + " is missing.") {}
  ~DelegationMissing() noexcept override = default;
};

class InvalidTarget : public Exception {
 public:
  InvalidTarget(const std::string& reponame)
      : Exception(reponame, "The target had a non-OSTree package that can not be installed on an OSTree system.") {}
  ~InvalidTarget() noexcept override = default;
};

}  // namespace Uptane

#endif
