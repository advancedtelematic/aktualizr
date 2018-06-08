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

namespace Uptane {

using KeyId = std::string;
/**
 * TUF Roles
 */
class Role {
 public:
  static Role Root() { return Role{RoleEnum::Root}; }
  static Role Snapshot() { return Role{RoleEnum::Snapshot}; }
  static Role Targets() { return Role{RoleEnum::Targets}; }
  static Role Timestamp() { return Role{RoleEnum::Timestamp}; }
  static Role InvalidRole() { return Role{RoleEnum::InvalidRole}; }
  explicit Role(const std::string & /*role_name*/);
  std::string ToString() const;
  bool operator==(const Role &other) const { return role_ == other.role_; }
  bool operator!=(const Role &other) const { return !(*this == other); }
  bool operator<(const Role &other) const { return role_ < other.role_; }

  friend std::ostream &operator<<(std::ostream &os, const Role &t);

 private:
  enum class RoleEnum { Root, Snapshot, Targets, Timestamp, InvalidRole };

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
  int version() { return version_; }

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
  enum class Type { Sha256, Sha512, UnknownAlgorithm };

  Hash(const std::string &type, const std::string &hash);
  Hash(Type type, const std::string &hash);

  bool HaveAlgorithm() const { return type_ != Type::UnknownAlgorithm; }
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
  EcuSerial ecu_identifier() const { return ecu_identifier_; }
  std::string filename() const { return filename_; }
  std::string format() const { return type_; }
  std::string sha256Hash() const;
  std::vector<Hash> hashes() const { return hashes_; };

  bool MatchWith(const Hash &hash) const;

  int64_t length() const { return length_; }
  Json::Value toJson() const;

  bool IsForSecondary(const EcuSerial &ecuIdentifier) const {
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
  EcuSerial ecu_identifier_{EcuSerial(EcuSerial::Unknown())};
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
  enum class Policy { RejectAll, AcceptAll, Check };
  /**
   * An empty Root, that either accepts or rejects everything
   */
  explicit Root(Policy policy = Policy::RejectAll) : policy_(policy) { version_ = 0; }
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
