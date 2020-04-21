#ifndef IMAGE_REPO_H_
#define IMAGE_REPO_H_

#include "repo.h"

class ImageRepo : public Repo {
 public:
  ImageRepo(boost::filesystem::path path, const std::string &expires, std::string correlation_id)
      : Repo(Uptane::RepositoryType::Image(), std::move(path), expires, std::move(correlation_id)) {}
  void addBinaryImage(const boost::filesystem::path &image_path, const boost::filesystem::path &targetname,
                      const std::string &hardware_id, const std::string &url, const Delegation &delegation = {});
  void addCustomImage(const std::string &name, const Hash &hash, uint64_t length, const std::string &hardware_id,
                      const std::string &url, const Delegation &delegation, const Json::Value &custom = {});
  void addDelegation(const Uptane::Role &name, const Uptane::Role &parent_role, const std::string &path,
                     bool terminating, KeyType key_type);
  void revokeDelegation(const Uptane::Role &name);
  std::vector<std::string> getDelegationTargets(const Uptane::Role &name);

  // note: it used to be "repo/image" which is way less confusing but we've just
  // given up and adopted what the backend does
  static constexpr const char *dir{"repo/repo"};

 private:
  void addImage(const std::string &name, Json::Value &target, const std::string &hardware_id,
                const Delegation &delegation = {});
  void removeDelegationRecursive(const Uptane::Role &name, const Uptane::Role &parent_name);
};

#endif  // IMAGE_REPO_H_
