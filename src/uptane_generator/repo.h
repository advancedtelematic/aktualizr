#ifndef REPO_H_
#define REPO_H_

#include <fnmatch.h>

#include <crypto/crypto.h>
#include <boost/filesystem.hpp>
#include "json/json.h"
#include "uptane/tuf.h"

struct KeyPair {
  KeyPair() = default;
  KeyPair(PublicKey public_key_in, std::string private_key_in)
      : public_key(std::move(public_key_in)), private_key(std::move(private_key_in)) {}
  PublicKey public_key;
  std::string private_key;
};

struct Delegation {
  Delegation() = default;
  Delegation(const boost::filesystem::path &repo_path, std::string delegation_name);
  bool isMatched(const boost::filesystem::path &image_path) const {
    return (fnmatch(pattern.c_str(), image_path.c_str(), 0) == 0);
  }
  operator bool() const { return (!name.empty() && !pattern.empty()); }
  std::string name;
  std::string pattern;

 private:
  static std::string findPatternInTree(const boost::filesystem::path &repo_path, const std::string &name,
                                       const Json::Value &targets_json);
};

class Repo {
 public:
  Repo(Uptane::RepositoryType repo_type, boost::filesystem::path path, const std::string &expires,
       std::string correlation_id);
  void generateRepo(KeyType key_type = KeyType::kRSA2048);
  Json::Value getTarget(const std::string &target_name);
  Json::Value signTuf(const Uptane::Role &role, const Json::Value &json);
  void generateCampaigns() const;
  void refresh(const Uptane::Role &role);

 protected:
  void generateRepoKeys(KeyType key_type);
  void generateKeyPair(KeyType key_type, const Uptane::Role &key_name);
  static std::string getExpirationTime(const std::string &expires);
  void readKeys();
  void updateRepo();
  Uptane::RepositoryType repo_type_;
  boost::filesystem::path path_;
  boost::filesystem::path repo_dir_;
  std::string correlation_id_;
  std::string expiration_time_;
  std::map<Uptane::Role, KeyPair> keys_;

 private:
  void addDelegationToSnapshot(Json::Value *snapshot, const Uptane::Role &role);
};

#endif  // REPO_H_
