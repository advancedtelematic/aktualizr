#ifndef UPTANE_TUFREPOSITORY_H_
#define UPTANE_TUFREPOSITORY_H_

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/filesystem.hpp>

#include <map>
#include <string>

#include <json/json.h>
#include "config.h"
#include "crypto.h"
#include "httpclient.h"
#include "uptane/exceptions.h"

namespace Uptane {
typedef std::map<std::string, unsigned int> RoleThreshold;

class Hasher {
 public:
  Hasher() {}
  enum Type { sha256 = 0, sha512 };
  Hasher(Type type, const std::string &hash) : type_(type), hash_(boost::algorithm::to_upper_copy(hash)) {}
  Type type_;
  std::string hash_;
  bool matchWith(const std::string &content) {
    if (type_ == sha256) {
      return hash_ == boost::algorithm::hex(Crypto::sha256digest(content));
    } else {
      return hash_ == boost::algorithm::hex(Crypto::sha512digest(content));
    }
  }
};

struct Target {
  Target(const Json::Value &custom, const std::string &filename, unsigned long long length, Hasher hash)
      : custom_(custom), filename_(filename), length_(length), hash_(hash) {}
  bool operator==(const Target &t2) {
    return (filename_ == t2.filename_ && length_ == t2.length_ && hash_.hash_ == t2.hash_.hash_);
  }
  Json::Value custom_;
  std::string filename_;
  unsigned long long length_;
  Hasher hash_;
};

class TufRepository {
 public:
  TufRepository(const std::string &name, const std::string &base_url, const Config &config);
  void verifyRole(const Json::Value &);
  void initRoot(const Json::Value &content);

  // all of the update* methods throw uptane::SecurityException if the signatures are incorrect
  void updateRoot();
  bool checkTimestamp();

  Json::Value getJSON(const std::string &role);
  Json::Value updateRole(const std::string &role);
  std::vector<Target> getTargets() { return targets_; }

  void refresh();

 private:
  static const int kMinSignatures = 1;
  static const int kMaxSignatures = 1000;

  void saveRole(const Json::Value &content);
  std::string downloadTarget(Target target);
  void saveTarget(const Target &target);
  bool hasExpired(const std::string &date);
  void updateKeys(const Json::Value &keys);
  bool findSignatureByKeyId(const Json::Value &signatures, const std::string &keyid);

  std::string name_;
  boost::filesystem::path path_;
  Config config_;
  std::map<std::string, PublicKey> keys_;
  RoleThreshold thresholds_;
  Json::Value timestamp_signed_;
  HttpClient http_;
  std::vector<Target> targets_;
  std::string base_url_;
  friend class TestBusSecondary;

  // TODO: list of targets
};
}

#endif