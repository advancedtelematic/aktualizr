
#include "uptane_repo.h"

UptaneRepo::UptaneRepo(const boost::filesystem::path &path, const std::string &expires,
                       const std::string &correlation_id)
    : director_repo_(path, expires, correlation_id), image_repo_(path, expires, correlation_id) {}

void UptaneRepo::generateRepo(KeyType key_type) {
  director_repo_.generateRepo(key_type);
  image_repo_.generateRepo(key_type);
}

void UptaneRepo::addTarget(const std::string &target_name, const std::string &hardware_id,
                           const std::string &ecu_serial, const std::string &url, const std::string &expires) {
  auto target = image_repo_.getTarget(target_name);
  if (target.empty()) {
    throw std::runtime_error("No such " + target_name + " target in the image repository");
  }
  director_repo_.addTarget(target_name, target, hardware_id, ecu_serial, url, expires);
}

void UptaneRepo::addDelegation(const Uptane::Role &name, const Uptane::Role &parent_role, const std::string &path,
                               bool terminating, KeyType key_type) {
  image_repo_.addDelegation(name, parent_role, path, terminating, key_type);
}

void UptaneRepo::revokeDelegation(const Uptane::Role &name) {
  director_repo_.revokeTargets(image_repo_.getDelegationTargets(name));
  image_repo_.revokeDelegation(name);
}

void UptaneRepo::addImage(const boost::filesystem::path &image_path, const boost::filesystem::path &targetname,
                          const std::string &hardware_id, const std::string &url, const Delegation &delegation) {
  image_repo_.addBinaryImage(image_path, targetname, hardware_id, url, delegation);
}
void UptaneRepo::addCustomImage(const std::string &name, const Hash &hash, uint64_t length,
                                const std::string &hardware_id, const std::string &url, const Delegation &delegation,
                                const Json::Value &custom) {
  image_repo_.addCustomImage(name, hash, length, hardware_id, url, delegation, custom);
}

void UptaneRepo::signTargets() { director_repo_.signTargets(); }

void UptaneRepo::emptyTargets() { director_repo_.emptyTargets(); }
void UptaneRepo::oldTargets() { director_repo_.oldTargets(); }

void UptaneRepo::generateCampaigns() { director_repo_.generateCampaigns(); }

void UptaneRepo::refresh(Uptane::RepositoryType repo_type, const Uptane::Role &role) {
  if (repo_type == Uptane::RepositoryType(Uptane::RepositoryType::Director())) {
    director_repo_.refresh(role);
  } else if (repo_type == Uptane::RepositoryType(Uptane::RepositoryType::Image())) {
    image_repo_.refresh(role);
  }
}
