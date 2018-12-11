#ifndef REPO_H_
#define REPO_H_

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

class RepoType {
 public:
  enum class Type { kDirector = 0, kImage };
  RepoType(RepoType::Type type) { type_ = type; }
  RepoType(const std::string &repo_type) {
    if (repo_type == "director") {
      type_ = RepoType::Type::kDirector;
    } else if (repo_type == "image") {
      type_ = RepoType::Type::kImage;
    } else {
      throw std::runtime_error(std::string("incorrect repo type: ") + repo_type);
    }
  }
  Type type_;
  bool operator==(const RepoType &other) { return type_ == other.type_; }
  std::string toString() {
    if (type_ == RepoType::Type::kDirector) {
      return "director";
    } else {
      return "image";
    }
  }
};

class Repo {
 public:
  Repo(RepoType repo_type, boost::filesystem::path path, const std::string &expires, std::string correlation_id);
  void generateRepo(KeyType key_type = KeyType::kRSA2048);
  Json::Value getTarget(const std::string &target_name);
  Json::Value signTuf(const Uptane::Role &role, const Json::Value &json);

 protected:
  void generateRepoKeys(KeyType key_type);
  void generateKeyPair(KeyType key_type, const Uptane::Role &key_name);
  std::string getExpirationTime(const std::string &expires);
  void readKeys();
  RepoType repo_type_;
  boost::filesystem::path path_;
  std::string correlation_id_;
  std::string expiration_time_;
  std::map<Uptane::Role, KeyPair> keys_;
};

#endif  // REPO_H_
