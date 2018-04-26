#ifndef AKTUALIZR_UPTANE_TUF_H_
#define AKTUALIZR_UPTANE_TUF_H_

/**
 * Base data types that are used in The Update Framework (TUF), part of UPTANE.
 */

#include <ostream>
#include <set>
#include "uptane/exceptions.h"

#include "crypto/crypto.h"

namespace Uptane {

using KeyId = std::string;
/**
 * TUF Roles
 */
class Role {
 public:
  static Role Root() { return Role{kRoot}; }
  static Role Snapshot() { return Role{kSnapshot}; }
  static Role Targets() { return Role{kTargets}; }
  static Role Timestamp() { return Role{kTimestamp}; }
  static Role InvalidRole() { return Role{kInvalidRole}; }
  explicit Role(const std::string & /*role_name*/);
  std::string ToString() const;
  bool operator==(const Role &other) const { return role_ == other.role_; }
  bool operator!=(const Role &other) const { return !(*this == other); }
  bool operator<(const Role &other) const { return role_ < other.role_; }

  friend std::ostream &operator<<(std::ostream &os, const Role &t);

 private:
  enum RoleEnum { kRoot, kSnapshot, kTargets, kTimestamp, kInvalidRole };

  explicit Role(RoleEnum role) : role_(role) {}

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
  Target(std::string filename, const Json::Value &content);

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
    if (filename_ != t2.filename_) {
      return false;
    }
    if (length_ != t2.length_) {
      return false;
    }

    // requirements:
    // - all hashes of the same type should match
    // - at least one pair of hashes should match
    bool oneMatchingHash = false;
    for (const Hash &hash : hashes_) {
      for (const Hash &hash2 : t2.hashes_) {
        if (hash.type() == hash2.type() && !(hash == hash2)) {
          return false;
        }
        if (hash == hash2) {
          oneMatchingHash = true;
        }
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
class Root;
class BaseMeta {
 public:
  BaseMeta() = default;
  ;
  explicit BaseMeta(const Json::Value &json);
  BaseMeta(const TimeStamp &now, const std::string &repository, const Json::Value &json, Root &root);
  int version() const { return version_; }
  TimeStamp expiry() const { return expiry_; }
  Json::Value original() const { return original_object_; }

  bool operator==(const BaseMeta &rhs) const { return version_ == rhs.version() && expiry_ == rhs.expiry(); }
  Json::Value toJson() const {
    Json::Value res;
    res["expires"] = expiry_.ToString();
    res["version"] = version_;
    return res;
  }

 protected:
  int version_ = {-1};
  TimeStamp expiry_;
  Json::Value original_object_;

 private:
  void init(const Json::Value &json);
};

// Implemented in uptane/root.cc
class Root : public BaseMeta {
 public:
  enum Policy { kRejectAll, kAcceptAll, kCheck };
  /**
   * An empty Root, that either accepts or rejects everything
   */
  explicit Root(Policy policy = kRejectAll) : policy_(policy) { version_ = 0; }
  /**
   * A 'real' root that implements TUF signature validation
   * @param repository - The name of the repository (only used to improve the error messages)
   * @param json - The contents of the 'signed' portion
   */
  Root(const std::string &repository, const Json::Value &json);
  Root(const TimeStamp &now, const std::string &repository, const Json::Value &json, Root &root);

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
  void UnpackSignedObject(const TimeStamp &now, const std::string &repository, const Json::Value &signed_object);
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
  std::map<KeyId, PublicKey> keys_;
  std::set<std::pair<Role, KeyId> > keys_for_role_;
  std::map<Role, int> thresholds_for_role_;
};

class Targets : public BaseMeta {
 public:
  explicit Targets(const Json::Value &json);
  Targets(const TimeStamp &now, const std::string &repository, const Json::Value &json, Root &root);
  Targets() = default;
  ;
  Json::Value toJson() const;

  std::vector<Uptane::Target> targets;
  bool operator==(const Targets &rhs) const {
    return version_ == rhs.version() && expiry_ == rhs.expiry() && targets == rhs.targets;
  }

 private:
  void init(const Json::Value &json);
};

class TimestampMeta : public BaseMeta {
 public:
  // TODO: add METAFILES section
  explicit TimestampMeta(const Json::Value &json) : BaseMeta(json) {}
  TimestampMeta(const TimeStamp &now, const std::string &repository, const Json::Value &json, Root &root)
      : BaseMeta(now, repository, json, root){};
  TimestampMeta() = default;
  ;
  Json::Value toJson() const;
};

class Snapshot : public BaseMeta {
 public:
  std::map<std::string, int> versions;
  explicit Snapshot(const Json::Value &json);
  Snapshot(const TimeStamp &now, const std::string &repository, const Json::Value &json, Root &root);
  Snapshot() = default;
  ;
  Json::Value toJson() const;
  bool operator==(const Snapshot &rhs) const {
    return version_ == rhs.version() && expiry_ == rhs.expiry() && versions == rhs.versions;
  }

 private:
  void init(const Json::Value &json);
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
