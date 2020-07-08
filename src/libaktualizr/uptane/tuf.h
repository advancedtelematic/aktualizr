#ifndef AKTUALIZR_UPTANE_TUF_H_
#define AKTUALIZR_UPTANE_TUF_H_

/**
 * Base data types that are used in The Update Framework (TUF), part of Uptane.
 */

#include <functional>
#include <map>
#include <ostream>
#include <set>
#include <unordered_map>
#include <vector>

#include "crypto/crypto.h"
#include "libaktualizr/types.h"
#include "uptane/exceptions.h"

namespace Uptane {

class RepositoryType {
 private:
  /** This must match the repo_type table in sqlstorage */
  enum class Type { kUnknown = -1, kImage = 0, kDirector = 1 };

 public:
  static const std::string IMAGE;
  static const std::string DIRECTOR;

  RepositoryType() = default;
  static constexpr int Director() { return static_cast<int>(Type::kDirector); }
  static constexpr int Image() { return static_cast<int>(Type::kImage); }
  RepositoryType(int type) { type_ = static_cast<RepositoryType::Type>(type); }
  RepositoryType(const std::string &repo_type) {
    if (repo_type == DIRECTOR) {
      type_ = RepositoryType::Type::kDirector;
    } else if (repo_type == IMAGE) {
      type_ = RepositoryType::Type::kImage;
    } else {
      throw std::runtime_error(std::string("Incorrect repo type: ") + repo_type);
    }
  }
  operator int() const { return static_cast<int>(type_); }
  operator std::string() const { return toString(); }
  Type type_;
  std::string toString() const {
    if (type_ == RepositoryType::Type::kDirector) {
      return DIRECTOR;
    } else if (type_ == RepositoryType::Type::kImage) {
      return IMAGE;
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
  static const std::string ROOT;
  static const std::string SNAPSHOT;
  static const std::string TARGETS;
  static const std::string TIMESTAMP;

  static Role Root() { return Role{RoleEnum::kRoot}; }
  static Role Snapshot() { return Role{RoleEnum::kSnapshot}; }
  static Role Targets() { return Role{RoleEnum::kTargets}; }
  static Role Timestamp() { return Role{RoleEnum::kTimestamp}; }
  static Role Delegation(const std::string &name) { return Role(name, true); }
  static Role InvalidRole() { return Role{RoleEnum::kInvalidRole}; }
  // Delegation is not included because this is only used for a metadata table
  // that doesn't include delegations.
  static std::vector<Role> Roles() { return {Root(), Snapshot(), Targets(), Timestamp()}; }
  static bool IsReserved(const std::string &name) {
    return (name == ROOT || name == TARGETS || name == SNAPSHOT || name == TIMESTAMP);
  }

  explicit Role(const std::string &role_name, bool delegation = false);
  std::string ToString() const;
  int ToInt() const { return static_cast<int>(role_); }
  bool IsDelegation() const { return role_ == RoleEnum::kDelegation; }
  bool operator==(const Role &other) const { return name_ == other.name_; }
  bool operator!=(const Role &other) const { return !(*this == other); }
  bool operator<(const Role &other) const { return name_ < other.name_; }

  friend std::ostream &operator<<(std::ostream &os, const Role &role);

 private:
  /** The four standard roles must match the meta_types table in sqlstorage.
   *  Delegations are special and handled differently. */
  enum class RoleEnum { kRoot = 0, kSnapshot = 1, kTargets = 2, kTimestamp = 3, kDelegation = 4, kInvalidRole = -1 };

  explicit Role(RoleEnum role) : role_(role) {
    if (role_ == RoleEnum::kRoot) {
      name_ = ROOT;
    } else if (role_ == RoleEnum::kSnapshot) {
      name_ = SNAPSHOT;
    } else if (role_ == RoleEnum::kTargets) {
      name_ = TARGETS;
    } else if (role_ == RoleEnum::kTimestamp) {
      name_ = TIMESTAMP;
    } else {
      role_ = RoleEnum::kInvalidRole;
      name_ = "invalidrole";
    }
  }

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
  int version() const { return version_; }
  bool operator==(const Version &rhs) const { return version_ == rhs.version_; }
  bool operator!=(const Version &rhs) const { return version_ != rhs.version_; }
  bool operator<(const Version &rhs) const { return version_ < rhs.version_; }

 private:
  static const int ANY_VERSION = -1;
  int version_;
  friend std::ostream &operator<<(std::ostream &os, const Version &v);
};

std::ostream &operator<<(std::ostream &os, const Version &v);

/* Metadata objects */
class MetaWithKeys;
class BaseMeta {
 public:
  BaseMeta() = default;
  explicit BaseMeta(const Json::Value &json);
  BaseMeta(RepositoryType repo, const Role &role, const Json::Value &json, const std::shared_ptr<MetaWithKeys> &signer);
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

class MetaWithKeys : public BaseMeta {
 public:
  enum class Policy { kRejectAll, kAcceptAll, kCheck };
  /**
   * An empty metadata object that could contain keys.
   */
  MetaWithKeys() { version_ = 0; }
  /**
   * A 'real' metadata object that can contain keys (Root or Targets with
   * delegations) and that implements TUF signature validation.
   * @param json - The contents of the 'signed' portion
   */
  MetaWithKeys(const Json::Value &json);
  MetaWithKeys(RepositoryType repo, const Role &role, const Json::Value &json,
               const std::shared_ptr<MetaWithKeys> &signer);

  virtual ~MetaWithKeys() = default;

  void ParseKeys(RepositoryType repo, const Json::Value &keys);
  // role is the name of a role described in this object's metadata.
  // meta_role is the name of this object's role.
  void ParseRole(RepositoryType repo, const Json::ValueConstIterator &it, const Role &role,
                 const std::string &meta_role);

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
   * @param role - The Uptane role of the signed metadata object
   * @param signed_object
   * @return
   */
  virtual void UnpackSignedObject(RepositoryType repo, const Role &role, const Json::Value &signed_object);

  bool operator==(const MetaWithKeys &rhs) const {
    return version_ == rhs.version_ && expiry_ == rhs.expiry_ && keys_ == rhs.keys_ &&
           keys_for_role_ == rhs.keys_for_role_ && thresholds_for_role_ == rhs.thresholds_for_role_;
  }

 protected:
  static const int64_t kMinSignatures = 1;
  static const int64_t kMaxSignatures = 1000;

  std::map<KeyId, PublicKey> keys_;
  std::set<std::pair<Role, KeyId>> keys_for_role_;
  std::map<Role, int64_t> thresholds_for_role_;
};

// Implemented in uptane/root.cc
class Root : public MetaWithKeys {
 public:
  /**
   * An empty Root, that either accepts or rejects everything
   */
  explicit Root(Policy policy = Policy::kRejectAll) : policy_(policy) { version_ = 0; }
  /**
   * A 'real' Root that implements TUF signature validation
   * @param repo - Repository type (only used to improve the error messages)
   * @param json - The contents of the 'signed' portion
   */
  Root(RepositoryType repo, const Json::Value &json);
  Root(RepositoryType repo, const Json::Value &json, Root &root);

  ~Root() override = default;

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
   * @param role - The Uptane role of the signed metadata object
   * @param signed_object
   * @return
   */
  void UnpackSignedObject(RepositoryType repo, const Role &role, const Json::Value &signed_object) override;

  bool operator==(const Root &rhs) const {
    return version_ == rhs.version_ && expiry_ == rhs.expiry_ && keys_ == rhs.keys_ &&
           keys_for_role_ == rhs.keys_for_role_ && thresholds_for_role_ == rhs.thresholds_for_role_ &&
           policy_ == rhs.policy_;
  }

 private:
  Policy policy_;
};

static bool MatchTargetVector(const std::vector<Uptane::Target> &v1, const std::vector<Uptane::Target> &v2) {
  if (v1.size() != v2.size()) {
    return false;
  }
  for (size_t i = 0; i < v1.size(); ++i) {
    if (!v1[i].MatchTarget(v2[i])) {
      return false;
    }
  }
  return true;
}

// Also used for delegated targets.
class Targets : public MetaWithKeys {
 public:
  explicit Targets(const Json::Value &json);
  Targets(RepositoryType repo, const Role &role, const Json::Value &json, const std::shared_ptr<MetaWithKeys> &signer);
  Targets() = default;
  ~Targets() override = default;

  bool operator==(const Targets &rhs) const {
    return version_ == rhs.version() && expiry_ == rhs.expiry() && MatchTargetVector(targets, rhs.targets);
  }

  const std::string &correlation_id() const { return correlation_id_; }

  void clear() {
    targets.clear();
    delegated_role_names_.clear();
    paths_for_role_.clear();
    terminating_role_.clear();
  }

  std::vector<Uptane::Target> getTargets(const Uptane::EcuSerial &ecu_id,
                                         const Uptane::HardwareIdentifier &hw_id) const {
    std::vector<Uptane::Target> result;
    for (auto it = targets.begin(); it != targets.end(); ++it) {
      auto found_loc = std::find_if(it->ecus().begin(), it->ecus().end(),
                                    [ecu_id, hw_id](const std::pair<EcuSerial, HardwareIdentifier> &val) {
                                      return ((ecu_id == val.first) && (hw_id == val.second));
                                    });

      if (found_loc != it->ecus().end()) {
        result.push_back(*it);
      }
    }
    return result;
  }

  std::vector<Uptane::Target> targets;
  std::vector<std::string> delegated_role_names_;
  std::map<Role, std::vector<std::string>> paths_for_role_;
  std::map<Role, bool> terminating_role_;

 private:
  void init(const Json::Value &json);

  std::string name_;
  std::string correlation_id_;  // custom non-tuf
};

class TimestampMeta : public BaseMeta {
 public:
  explicit TimestampMeta(const Json::Value &json);
  TimestampMeta(RepositoryType repo, const Json::Value &json, const std::shared_ptr<MetaWithKeys> &signer);
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
  Snapshot(RepositoryType repo, const Json::Value &json, const std::shared_ptr<MetaWithKeys> &signer);
  Snapshot() = default;
  std::vector<Hash> role_hashes(const Uptane::Role &role) const;
  int64_t role_size(const Uptane::Role &role) const;
  int role_version(const Uptane::Role &role) const;
  bool operator==(const Snapshot &rhs) const {
    return version_ == rhs.version() && expiry_ == rhs.expiry() && role_size_ == rhs.role_size_ &&
           role_version_ == rhs.role_version_ && role_hashes_ == rhs.role_hashes_;
  }

 private:
  void init(const Json::Value &json);
  std::map<Uptane::Role, int64_t> role_size_;
  std::map<Uptane::Role, int> role_version_;
  std::map<Uptane::Role, std::vector<Hash>> role_hashes_;
};

struct MetaPairHash {
  std::size_t operator()(const std::pair<RepositoryType, Role> &pair) const {
    return std::hash<std::string>()(pair.first.toString()) ^ std::hash<std::string>()(pair.second.ToString());
  }
};

using MetaBundle = std::unordered_map<std::pair<RepositoryType, Role>, std::string, MetaPairHash>;

std::string getMetaFromBundle(const MetaBundle &bundle, RepositoryType repo, const Role &role);

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
