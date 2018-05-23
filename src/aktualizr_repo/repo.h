#ifndef REPO_H_
#define REPO_H_

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
  void generateRepo(const std::string &name);
  Json::Value signTuf(const Json::Value &json, const std::string &key);
  std::string getExpirationTime(const std::string &expires);

  boost::filesystem::path path_;
  std::string expiration_time_;
};

#endif  // REPO_H_