#ifndef AKTUALIZR_UPTANE_TUF_H_
#define AKTUALIZR_UPTANE_TUF_H_

/**
 * Base data types that are used in The Update Framework (TUF), part of UPTANE.
 */

#include <ostream>
#include <set>

#include "crypto.h"

namespace Uptane {

typedef std::string KeyId;
/**
 * TUF Roles
 */
class Role {
 public:
  static Role Root() { return Role(kRoot); }
  static Role Snapshot() { return Role(kSnapshot); }
  static Role Targets() { return Role(kTargets); }
  static Role Timestamp() { return Role(kTimestamp); }
  static Role InvalidRole() { return Role(kInvalidRole); }
  explicit Role(const std::string &);
  std::string ToString() const;
  bool operator==(const Role &other) const { return role_ == other.role_; }
  bool operator!=(const Role &other) const { return !(*this == other); }
  bool operator<(const Role &other) const { return role_ < other.role_; }

  friend std::ostream &operator<<(std::ostream &os, const Role &t);

 private:
  enum RoleEnum { kRoot, kSnapshot, kTargets, kTimestamp, kInvalidRole };

  Role(RoleEnum role) : role_(role) {}

  RoleEnum role_;
};

std::ostream &operator<<(std::ostream &os, const Role &t);

/**
 * Metadata version numbers
 */
class Version {
 public:
  Version() : version_(ANY_VERSION) {}
  explicit Version(int v) : version_(v) {}
  std::string RoleFileName(Role role) const;

 private:
  static const int ANY_VERSION = -1;
  int version_;
  friend std::ostream &operator<<(std::ostream &os, const Version &v);
};

std::ostream &operator<<(std::ostream &os, const Version &v);

class TimeStamp {
 public:
  static TimeStamp Now();
  /** An invalid TimeStamp */
  TimeStamp() { ; }
  explicit TimeStamp(std::string rfc3339);
  bool IsExpiredAt(const TimeStamp &now) const;
  bool IsValid() const;
  std::string ToString() const { return time_; }
  bool operator<(const TimeStamp &other) const;
  bool operator>(const TimeStamp &other) const;
  friend std::ostream &operator<<(std::ostream &os, const TimeStamp &t);
  bool operator==(const TimeStamp &rhs) const { return time_ == rhs.time_; }

 private:
  std::string time_;
};

std::ostream &operator<<(std::ostream &os, const TimeStamp &t);

/**
 * The hash of a file or TUF metadata.  File hashes/checksums in TUF include the length of the object, in order to
 * defeat infinite download attacks.
 */
class Hash {
 public:
  enum Type { kSha256, kSha512, kUnknownAlgorithm };

  Hash(const std::string &type, const std::string &hash);
  Hash(Type type, const std::string &hash);

  bool HaveAlgorithm() const { return type_ != kUnknownAlgorithm; }
  bool operator==(const Hash &other) const;
  std::string TypeString() const;
  Type type() const;
  std::string HashString() const { return hash_; }
  friend std::ostream &operator<<(std::ostream &os, const Hash &h);

 private:
  Type type_;
  std::string hash_;
};

std::ostream &operator<<(std::ostream &os, const Hash &h);

class Target {
 public:
  Target(const std::string &filename, const Json::Value &content);

  // TODO: ECU HW ID
  std::string ecu_identifier() const { return ecu_identifier_; }
  std::string filename() const { return filename_; }
  std::string format() const { return type_; }
  std::string sha256Hash() const;
  std::vector<Hash> hashes() const { return hashes_; };

  bool MatchWith(const Hash &hash) const;

  int64_t length() const { return length_; }
  Json::Value toJson() const;

  bool IsForSecondary(const std::string &ecuIdentifier) const {
    return (length() > 0) && (ecu_identifier() == ecuIdentifier);
  };

  bool operator==(const Target &t2) const {
    // if (type_ != t2.type_) return false; // Director doesn't include targetFormat
    if (filename_ != t2.filename_) return false;
    if (length_ != t2.length_) return false;

    // requirements:
    // - all hashes of the same type should match
    // - at least one pair of hashes should match
    bool oneMatchingHash = false;
    for (const Hash &hash : hashes_) {
      for (const Hash &hash2 : t2.hashes_) {
        if (hash.type() == hash2.type() && !(hash == hash2)) return false;
        if (hash == hash2) oneMatchingHash = true;
      }
    }
    return oneMatchingHash;
  }

  friend std::ostream &operator<<(std::ostream &os, const Target &t);

 private:
  std::string filename_;
  std::string type_;
  std::string ecu_identifier_;
  std::vector<Hash> hashes_;
  int64_t length_;
};

std::ostream &operator<<(std::ostream &os, const Target &t);

/* Metadata objects */

// Implemented in uptane/root.cc
class Root {
 public:
  enum Policy { kRejectAll, kAcceptAll, kCheck };
  /**
   * An empty Root, that either accepts or rejects everything
   */
  Root(Policy policy = kRejectAll) : policy_(policy), version_(0) {}
  /**
   * A 'real' root that implements TUF signature validation
   * @param repository - The name of the repository (only used to improve the error messages)
   * @param json - The contents of the 'signed' portion
   */
  Root(const std::string &repository, const Json::Value &json);
  /**
   * Take a JSON blob that contains a signatures/signed component that is supposedly for a given role, and check that is
   * suitably signed.
   * If it is, it returns the contents of the 'signed' part.
   *
   * It performs the following checks:
   * * "_type" matches the given role
   * * "expires" is in the past (vs 'now')
   * * The blob has valid signatures from enough keys to cross the threshold for this role
   * @param now - The current time (for signature expiry)
   * @param repository - The name of the repository (only used to improve the error messages)
   * @param role
   * @param signed_object
   * @return
   */
  Json::Value UnpackSignedObject(TimeStamp now, std::string repository, Role role, const Json::Value &signed_object);
  int version() const { return version_; }
  Json::Value toJson() const;
  bool operator==(const Root &rhs) const {
    return version_ == rhs.version_ && expiry_ == rhs.expiry_ && keys_ == rhs.keys_ &&
           keys_for_role_ == rhs.keys_for_role_ && thresholds_for_role_ == rhs.thresholds_for_role_ &&
           policy_ == rhs.policy_;
  }

 private:
  static const int kMinSignatures = 1;
  static const int kMaxSignatures = 1000;

  Policy policy_;
  int version_;
  TimeStamp expiry_;
  std::map<KeyId, PublicKey> keys_;
  std::set<std::pair<Role, KeyId> > keys_for_role_;
  std::map<Role, int> thresholds_for_role_;
};

class Targets {
 public:
  Targets(const Json::Value &json);
  Targets();
  Json::Value toJson() const;
  int version;
  TimeStamp expiry;

  std::vector<Uptane::Target> targets;
  bool operator==(const Targets &rhs) const {
    return version == rhs.version && expiry == rhs.expiry && targets == rhs.targets;
  }
};

class TimestampMeta {
 public:
  int version;
  TimeStamp expiry;
  // TODO: add METAFILES section

  TimestampMeta(const Json::Value &json);
  TimestampMeta();
  Json::Value toJson() const;
  bool operator==(const TimestampMeta &rhs) const { return version == rhs.version && expiry == rhs.expiry; }
};

class Snapshot {
 public:
  int version;
  TimeStamp expiry;
  std::map<std::string, int> versions;

  Snapshot(const Json::Value &json);
  Snapshot();
  Json::Value toJson() const;
  bool operator==(const Snapshot &rhs) const {
    return version == rhs.version && expiry == rhs.expiry && versions == rhs.versions;
  }
};

struct MetaPack {
  Root director_root;
  Targets director_targets;
  Root image_root;
  Targets image_targets;
  TimestampMeta image_timestamp;
  Snapshot image_snapshot;
};
}  // namespace Uptane
#endif  // AKTUALIZR_UPTANE_TUF_H_
