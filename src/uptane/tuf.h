/**
 * Base data types that are used in The Update Framework (TUF), part of UPTANE.
 */

#ifndef AKTUALIZR_UPTANE_TUF_H_
#define AKTUALIZR_UPTANE_TUF_H_

#include <ostream>
#include <set>

#include "crypto.h"

namespace Uptane {

typedef std::string KeyId;
/**
 * TUF Roles
 */
enum Role { kRoot, kSnapshot, kTargets, kTimestamp, kInvalidRole };

Role RoleFromString(const std::string &rolename);

std::string StringFromRole(Role role);

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

class TimeStamp {
 public:
  static TimeStamp Now();
  /** An invalid TimeStamp */
  TimeStamp() {}
  explicit TimeStamp(std::string rfc3339);
  bool operator<(const TimeStamp &other) const;
  bool operator>(const TimeStamp &other) const;
  friend std::ostream &operator<<(std::ostream &os, const TimeStamp &t);
  bool valid() const;

 private:
  std::string time_;
};

/**
 * The hash of a file or TUF metadata.  File hashes/checksums in TUF include the length of the object, in order to
 * defeat infinite download attacks.
 */
class Hash {
 public:
  enum Type { sha256, sha512 };

  Hash(Type type, const std::string &hash);

  /**
   * Hash content and check it
   * @param content
   * @return
   */
  bool matchWith(const std::string &content) const;

  bool operator==(const Hash &other) const;

 private:
  Type type_;
  std::string hash_;
};

class Target {
 public:
  Target(const Json::Value &custom, const std::string &filename, Hash hash, int64_t length)
      : custom_(custom), filename_(filename), hash_(hash), length_(length) {}
  bool operator==(const Target &t2) const { return (filename_ == t2.filename_ && hash_ == t2.hash_); }

  std::string ecu_identifier() const { return custom_["ecuIdentifier"].asString(); }

  std::string filename() const { return filename_; }

  bool MatchWith(const std::string &content) const {
    // TODO: We should have some priority/ordering here
    return content.length() == length_ && hash_.matchWith(content);
  }

  int64_t length() const { return length_; }

  bool IsForSecondary(const std::string &ecuIdentifier) const {
    return length() > 0 && ecu_identifier() == ecuIdentifier;
  };

  friend std::ostream &operator<<(std::ostream &os, const Target &t);

 private:
  Json::Value custom_;
  std::string filename_;
  Hash hash_;
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
