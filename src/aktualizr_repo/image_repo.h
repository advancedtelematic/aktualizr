#ifndef IMAGE_REPO_H_
#define IMAGE_REPO_H_

#include "repo.h"

class ImageRepo : public Repo {
 public:
  ImageRepo(boost::filesystem::path path, const std::string &expires, std::string correlation_id)
      : Repo(Uptane::RepositoryType::Image(), std::move(path), expires, std::move(correlation_id)) {}
  void addImage(const boost::filesystem::path &image_path);
  void addImage(const std::string &name, const std::string &hash, uint64_t length);

 private:
  void addImage(const std::string &name, const Json::Value &target);
};

#endif  // IMAGE_REPO_H_