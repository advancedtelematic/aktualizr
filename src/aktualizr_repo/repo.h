#ifndef REPO_H_
#define REPO_H_

#include <crypto/crypto.h>
#include <boost/filesystem.hpp>
#include "json/json.h"

class Repo {
 public:
  Repo(boost::filesystem::path path, const std::string &expires);
  void generateRepo();
  void addImage(const boost::filesystem::path &image_path);
  void copyTarget(const std::string &target_name);

 private:
  void generateKeys();
  void generateRepo(const std::string &repo_type);
  Json::Value signTuf(const std::string &repo_type, const Json::Value &json);
  std::string getExpirationTime(const std::string &expires);

  PublicKey GetPublicKey(const std::string &repo_type) const;
  boost::filesystem::path path_;
  std::string expiration_time_;
};

#endif  // REPO_H_