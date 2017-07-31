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
  friend std::ostream &operator<<(std::ostream &os, const Version &t);
};

std::ostream &operator<<(std::ostream &os, const Version &t);

class TimeStamp {
 public:
  static TimeStamp Now();
  /** An invalid TimeStamp */
  TimeStamp() {}
  explicit TimeStamp(std::string rfc3339);
  bool IsExpiredAt(const TimeStamp &now) const;
  bool IsValid() const;
  bool operator<(const TimeStamp &other) const;
  bool operator>(const TimeStamp &other) const;
  friend std::ostream &operator<<(std::ostream &os, const TimeStamp &t);

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

  /**
   * Hash content and check it
   * @param content
   * @return
   */

  bool HaveAlgorithm() const { return type_ != kUnknownAlgorithm; }
  bool operator==(const Hash &other) const;
  friend std::ostream &operator<<(std::ostream &os, const Hash &h);

 private:
  Type type_;
  std::string hash_;
};

std::ostream &operator<<(std::ostream &os, const Hash &h);

class Target {
 public:
  Target(const std::string &name, const Json::Value &content);

  std::string ecu_identifier() const { return ecu_identifier_; }
  std::string filename() const { return filename_; }
  std::string format() const { return type_; }

  bool MatchWith(const Hash &hash) const;

  int64_t length() const { return length_; }

  bool IsForSecondary(const std::string &ecuIdentifier) const {
    return (length() > 0) && (ecu_identifier() == ecuIdentifier);
  };

  bool operator==(const Target &t2) {
    if (filename_ == t2.filename_ && length_ == t2.length_) {
      for (std::vector<Hash>::iterator it = hashes_.begin(); it != hashes_.end(); ++it) {
        if (t2.MatchWith(*it)) {
          return true;
        }
      }
    }
    return false;
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

class Root {
 public:
  enum Policy { kRejectAll, kAcceptAll, kCheck };
  /**
   * An empty Root, that either accepts or rejects everything
   */
  Root(Policy policy) : policy_(policy) {}
  /**
   * json should be the contents of the 'signed' portion
   * @param json
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
   * @param role
   * @param signed_object
   * @return
   */
  Json::Value UnpackSignedObject(TimeStamp now, std::string repository, Role role, const Json::Value &signed_object);

 private:
  static const int kMinSignatures = 1;
  static const int kMaxSignatures = 1000;

  Policy policy_;
  TimeStamp expiry_;
  std::map<KeyId, PublicKey> keys_;
  std::set<std::pair<Role, KeyId> > keys_for_role_;
  std::map<Role, int> thresholds_for_role_;
};
};

#endif  // AKTUALIZR_UPTANE_TUF_H_
