#ifndef UPTANE_REPO_H_
#define UPTANE_REPO_H_

#include "director_repo.h"
#include "image_repo.h"

class UptaneRepo {
 public:
  UptaneRepo(const boost::filesystem::path &path, const std::string &expires, const std::string &correlation_id);
  void generateRepo(KeyType key_type = KeyType::kRSA2048);
  void addTarget(const std::string &target_name, const std::string &hardware_id, const std::string &ecu_serial);
  void addImage(const boost::filesystem::path &image_path, const Delegation &delegation);
  void addDelegation(const Uptane::Role &name, const std::string &path, KeyType key_type = KeyType::kRSA2048);
  void addCustomImage(const std::string &name, const Uptane::Hash &hash, uint64_t length);
  void signTargets();
  void emptyTargets();
  void oldTargets();

 private:
  DirectorRepo director_repo_;
  ImageRepo image_repo_;
};

#endif  // UPTANE_REPO_H_
