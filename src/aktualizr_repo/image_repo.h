#ifndef IMAGE_REPO_H_
#define IMAGE_REPO_H_

#include "repo.h"

class ImageRepo : public Repo {
 public:
  ImageRepo(boost::filesystem::path path, const std::string &expires, std::string correlation_id)
      : Repo(Uptane::RepositoryType::Image(), std::move(path), expires, std::move(correlation_id)) {}
  void addImage(const boost::filesystem::path &image_path, const Delegation &delegation = {});
  void addCustomImage(const std::string &name, const Uptane::Hash &hash, uint64_t length);
  void addDelegation(const Uptane::Role &name, const std::string &path, KeyType key_type = KeyType::kRSA2048);

 private:
  void addImage(const std::string &name, const Json::Value &target, const Delegation &delegation = {});
};

#endif  // IMAGE_REPO_H_