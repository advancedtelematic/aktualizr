#ifndef AKTUALIZR_UPTANE_TUF_H_
#define AKTUALIZR_UPTANE_TUF_H_

/**
 * Base data types that are used in The Update Framework (TUF), part of UPTANE.
 */

#include <functional>
#include <ostream>
#include <set>
#include "uptane/exceptions.h"

#include "crypto/crypto.h"
#include "utilities/types.h"

namespace Uptane {

class RepositoryType {
 private:
  /** This must match the repo_type table in sqlstorage */
  enum class Type { kUnknown = -1, kImage = 0, kDirector = 1 };

 public:
  RepositoryType() = default;
  static constexpr int Director() { return static_cast<int>(Type::kDirector); }
  static constexpr int Image() { return static_cast<int>(Type::kImage); }
  RepositoryType(int type) { type_ = static_cast<RepositoryType::Type>(type); }
  RepositoryType(const std::string &repo_type) {
    if (repo_type == "director") {
      type_ = RepositoryType::Type::kDirector;
    } else if (repo_type == "image") {
      type_ = RepositoryType::Type::kImage;
    } else {
      throw std::runtime_error(std::string("Incorrect repo type: ") + repo_type);
    }
  }
  operator int() const { return static_cast<int>(type_); }
  operator const std::string() const { return toString(); }
  Type type_;
  std::string toString() const {
    if (type_ == RepositoryType::Type::kDirector) {
      return "director";
    } else if (type_ == RepositoryType::Type::kImage) {
      return "image";
    } else {
      return "";
    }
  }
};

using KeyId = std::string;
/**
 * TUF Roles
 */
class Role {
 public:
  static Role Root() { return Role{RoleEnum::kRoot}; }
  static Role Snapshot() { return Role{RoleEnum::kSnapshot}; }
  static Role Targets() { return Role{RoleEnum::kTargets}; }
  static Role Timestamp() { return Role{RoleEnum::kTimestamp}; }
  static Role Delegated(const std::string &name) { return Role(name, true); }
  static Role InvalidRole() { return Role{RoleEnum::kInvalidRole}; }
  static std::vector<Role> Roles() { return {Root(), Snapshot(), Targets(), Timestamp()}; }

  explicit Role(const std::string &role_name, bool delegation = false);
  std::string ToString() const;
  int ToInt() const { return static_cast<int>(role_); }
  bool operator==(const Role &other) const { return role_ == other.role_; }
  bool operator!=(const Role &other) const { return !(*this == other); }
  bool operator<(const Role &other) const { return role_ < other.role_; }

  friend std::ostream &operator<<(std::ostream &os, const Role &role);

 private:
  /** This must match the meta_types table in sqlstorage */
  enum class RoleEnum { kRoot = 0, kSnapshot = 1, kTargets = 2, kTimestamp = 3, kDelegated = 4, kInvalidRole = -1 };

  explicit Role(RoleEnum role) : role_(role) {}

  RoleEnum role_;
  std::string name_;
};

std::ostream &operator<<(std::ostream &os, const Role &role);

/**
 * Metadata version numbers
 */
class Version {
 public:
  Version() : version_(ANY_VERSION) {}
  explicit Version(int v) : version_(v) {}
  std::string RoleFileName(const Role &role) const;
  int version() { return version_; }

 private:
  static const int ANY_VERSION = -1;
  int version_;
  friend std::ostream &operator<<(std::ostream &os, const Version &v);
};

std::ostream &operator<<(std::ostream &os, const Version &v);

class HardwareIdentifier {
 public:
  // https://github.com/advancedtelematic/ota-tuf/blob/master/libtuf/src/main/scala/com/advancedtelematic/libtuf/data/TufDataType.scala
  static const int kMinLength = 0;
  static const int kMaxLength = 200;

  static HardwareIdentifier Unknown() { return HardwareIdentifier("Unknown"); }
  explicit HardwareIdentifier(const std::string &hwid) : hwid_(hwid) {
    /* if (hwid.length() < kMinLength) {
      throw std::out_of_range("Hardware Identifier too short");
    } */
    if (kMaxLength < hwid.length()) {
      throw std::out_of_range("Hardware Identifier too long");
    }
  }

  std::string ToString() const { return hwid_; }

  bool operator==(const HardwareIdentifier &rhs) const { return hwid_ == rhs.hwid_; }
  bool operator!=(const HardwareIdentifier &rhs) const { return !(*this == rhs); }

  bool operator<(const HardwareIdentifier &rhs) const { return hwid_ < rhs.hwid_; }
  friend std::ostream &operator<<(std::ostream &os, const HardwareIdentifier &hwid);
  friend struct std::hash<Uptane::HardwareIdentifier>;

 private:
  std::string hwid_;
};

std::ostream &operator<<(std::ostream &os, const HardwareIdentifier &hwid);

class EcuSerial {
 public:
  // https://github.com/advancedtelematic/ota-tuf/blob/master/libtuf/src/main/scala/com/advancedtelematic/libtuf/data/TufDataType.scala
  static const int kMinLength = 1;
  static const int kMaxLength = 64;

  static EcuSerial Unknown() { return EcuSerial("Unknown"); }
  explicit EcuSerial(const std::string &ecu_serial) : ecu_serial_(ecu_serial) {
    if (ecu_serial.length() < kMinLength) {
      throw std::out_of_range("Ecu serial identifier is too short");
    }
    if (kMaxLength < ecu_serial.length()) {
      throw std::out_of_range("Ecu serial identifier is too long");
    }
  }

  std::string ToString() const { return ecu_serial_; }

  bool operator==(const EcuSerial &rhs) const { return ecu_serial_ == rhs.ecu_serial_; }
  bool operator!=(const EcuSerial &rhs) const { return !(*this == rhs); }

  bool operator<(const EcuSerial &rhs) const { return ecu_serial_ < rhs.ecu_serial_; }
  friend std::ostream &operator<<(std::ostream &os, const EcuSerial &ecu_serial);
  friend struct std::hash<Uptane::EcuSerial>;

 private:
  std::string ecu_serial_;
};

std::ostream &operator<<(std::ostream &os, const EcuSerial &ecu_serial);

/**
 * The hash of a file or TUF metadata.  File hashes/checksums in TUF include the length of the object, in order to
 * defeat infinite download attacks.
 */
class Hash {
 public:
  // order corresponds algorithm priority
  enum class Type { kSha256, kSha512, kUnknownAlgorithm };

  Hash(const std::string &type, const std::string &hash);
  Hash(Type type, const std::string &hash);

  bool HaveAlgorithm() const { return type_ != Type::kUnknownAlgorithm; }
  bool operator==(const Hash &other) const;
  bool operator!=(const Hash &other) const { return !operator==(other); }
  std::string TypeString() const;
  Type type() const;
  std::string HashString() const { return hash_; }
  friend std::ostream &operator<<(std::ostream &os, const Hash &h);

  static std::string encodeVector(const std::vector<Uptane::Hash> &hashes);
  static std::vector<Uptane::Hash> decodeVector(std::string hashes_str);

 private:
  Type type_;
  std::string hash_;
};

std::ostream &operator<<(std::ostream &os, const Hash &h);

class Target {
 public:
  // From Uptane metadata
  Target(std::string filename, const Json::Value &content);
  // Internal, does not have type or ecu types informations
  Target(std::string filename, std::vector<Hash> hashes, uint64_t length, std::string correlation_id = "");

  static Target Unknown();

  const std::map<EcuSerial, HardwareIdentifier> &ecus() const { return ecus_; }
  std::string filename() const { return filename_; }
  std::string sha256Hash() const;
  std::vector<Hash> hashes() const { return hashes_; };
  std::vector<HardwareIdentifier> hardwareIds() const { return hwids_; };
  std::string custom_version() const { return custom_version_; }
  std::string correlation_id() const { return correlation_id_; };
  void setCorrelationId(std::string correlation_id) { correlation_id_ = std::move(correlation_id); };

  bool MatchWith(const Hash &hash) const;

  uint64_t length() const { return length_; }

  bool IsValid() const { return valid; }

  bool IsForSecondary(const EcuSerial &ecuIdentifier) const {
    return (std::find_if(ecus_.cbegin(), ecus_.cend(), [&ecuIdentifier](std::pair<EcuSerial, HardwareIdentifier> pair) {
              return pair.first == ecuIdentifier;
            }) != ecus_.cend());
  };

  /**
   * Is this an OSTree target?
   * OSTree targets need special treatment because the hash doesn't represent
   * the contents of the update itself, instead it is the hash (name) of the
   * root commit object.
   */
  bool IsOstree() const;

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

  Json::Value toDebugJson() const;
  friend std::ostream &operator<<(std::ostream &os, const Target &t);

 private:
  bool valid{true};
  std::string filename_;
  std::string type_;
  std::map<EcuSerial, HardwareIdentifier> ecus_;
  std::vector<Hash> hashes_;
  std::vector<HardwareIdentifier> hwids_;
  std::string custom_version_;
  uint64_t length_{0};
  std::string correlation_id_;
};

std::ostream &operator<<(std::ostream &os, const Target &t);

/* Metadata objects */
class Root;
class BaseMeta {
 public:
  BaseMeta() = default;
  explicit BaseMeta(const Json::Value &json);
  BaseMeta(RepositoryType repo, const Json::Value &json, Root &root);
  int version() const { return version_; }
  TimeStamp expiry() const { return expiry_; }
  bool isExpired(const TimeStamp &now) const { return expiry_.IsExpiredAt(now); }
  Json::Value original() const { return original_object_; }

  bool operator==(const BaseMeta &rhs) const { return version_ == rhs.version() && expiry_ == rhs.expiry(); }

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
  enum class Policy { kRejectAll, kAcceptAll, kCheck };
  /**
   * An empty Root, that either accepts or rejects everything
   */
  explicit Root(Policy policy = Policy::kRejectAll) : policy_(policy) { version_ = 0; }
  /**
   * A 'real' root that implements TUF signature validation
   * @param repo - Repository type (only used to improve the error messages)
   * @param json - The contents of the 'signed' portion
   */
  Root(RepositoryType repo, const Json::Value &json);
  Root(RepositoryType repo, const Json::Value &json, Root &root);

  /**
   * Take a JSON blob that contains a signatures/signed component that is supposedly for a given role, and check that is
   * suitably signed.
   * If it is, it returns the contents of the 'signed' part.
   *
   * It performs the following checks:
   * * "_type" matches the given role
   * * "expires" is in the past (vs 'now')
   * * The blob has valid signatures from enough keys to cross the threshold for this role
   * @param repo - Repository type (only used to improve the error messages)
   * @param signed_object
   * @return
   */
  void UnpackSignedObject(RepositoryType repo, const Json::Value &signed_object);
  bool operator==(const Root &rhs) const {
    return version_ == rhs.version_ && expiry_ == rhs.expiry_ && keys_ == rhs.keys_ &&
           keys_for_role_ == rhs.keys_for_role_ && thresholds_for_role_ == rhs.thresholds_for_role_ &&
           policy_ == rhs.policy_;
  }

 private:
  static const int64_t kMinSignatures = 1;
  static const int64_t kMaxSignatures = 1000;

  Policy policy_;
  std::map<KeyId, PublicKey> keys_;
  std::set<std::pair<Role, KeyId> > keys_for_role_;
  std::map<Role, int64_t> thresholds_for_role_;
};

class Targets : public BaseMeta {
 public:
  explicit Targets(const Json::Value &json);
  Targets(RepositoryType repo, const Json::Value &json, Root &root);
  Targets() = default;

  std::vector<Uptane::Target> targets;
  bool operator==(const Targets &rhs) const {
    return version_ == rhs.version() && expiry_ == rhs.expiry() && targets == rhs.targets;
  }
  const std::string &correlation_id() const { return correlation_id_; }

 private:
  void init(const Json::Value &json);

  std::string correlation_id_;  // custom non-tuf
};

class TimestampMeta : public BaseMeta {
 public:
  explicit TimestampMeta(const Json::Value &json);
  TimestampMeta(RepositoryType repo, const Json::Value &json, Root &root);
  TimestampMeta() = default;
  std::vector<Hash> snapshot_hashes() const { return snapshot_hashes_; };
  int64_t snapshot_size() const { return snapshot_size_; };
  int snapshot_version() const { return snapshot_version_; };

 private:
  void init(const Json::Value &json);

  std::vector<Hash> snapshot_hashes_;
  int64_t snapshot_size_{0};
  int snapshot_version_{-1};
};

class Snapshot : public BaseMeta {
 public:
  explicit Snapshot(const Json::Value &json);
  Snapshot(RepositoryType repo, const Json::Value &json, Root &root);
  Snapshot() = default;
  std::vector<Hash> targets_hashes() const { return targets_hashes_; };
  int64_t targets_size() const { return targets_size_; };
  int targets_version() const { return targets_version_; };
  bool operator==(const Snapshot &rhs) const {
    return version_ == rhs.version() && expiry_ == rhs.expiry() && targets_size_ == rhs.targets_size_ &&
           targets_version_ == rhs.targets_version_ && targets_hashes_ == rhs.targets_hashes_;
  }

 private:
  void init(const Json::Value &json);
  int64_t targets_size_{0};
  int targets_version_{-1};
  std::vector<Hash> targets_hashes_;
};

struct MetaPack {
  Root director_root;
  Targets director_targets;
  Root image_root;
  Targets image_targets;
  TimestampMeta image_timestamp;
  Snapshot image_snapshot;
  bool isConsistent() const;
};

struct RawMetaPack {
  std::string director_root;
  std::string director_targets;
  std::string image_root;
  std::string image_targets;
  std::string image_timestamp;
  std::string image_snapshot;
};

int extractVersionUntrusted(const std::string &meta);  // returns negative number if parsing fails

}  // namespace Uptane

namespace std {
template <>
struct hash<Uptane::HardwareIdentifier> {
  size_t operator()(const Uptane::HardwareIdentifier &hwid) const { return std::hash<std::string>()(hwid.hwid_); }
};

template <>
struct hash<Uptane::EcuSerial> {
  size_t operator()(const Uptane::EcuSerial &ecu_serial) const {
    return std::hash<std::string>()(ecu_serial.ecu_serial_);
  }
};
}  // namespace std

#endif  // AKTUALIZR_UPTANE_TUF_H_
