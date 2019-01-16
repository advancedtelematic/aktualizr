
#include "uptane_repo.h"

UptaneRepo::UptaneRepo(const boost::filesystem::path &path, const std::string &expires,
                       const std::string &correlation_id)
    : director_repo_(path, expires, correlation_id), image_repo_(path, expires, correlation_id) {}

void UptaneRepo::generateRepo(KeyType key_type) {
  director_repo_.generateRepo(key_type);
  image_repo_.generateRepo(key_type);
}
void UptaneRepo::addTarget(const std::string &target_name, const std::string &hardware_id,
                           const std::string &ecu_serial) {
  auto target = image_repo_.getTarget(target_name);
  if (target == Json::nullValue) {
    throw std::runtime_error("No such " + target_name + " target in the image repository");
  }
  director_repo_.addTarget(target_name, target, hardware_id, ecu_serial);
}

void UptaneRepo::addDelegation(const Uptane::Role &name, const std::string &path, KeyType key_type) {
  image_repo_.addDelegation(name, path, key_type);
}

void UptaneRepo::addImage(const boost::filesystem::path &image_path, const Delegation &delegation) {
  image_repo_.addImage(image_path, delegation);
}
void UptaneRepo::addCustomImage(const std::string &name, const Uptane::Hash &hash, uint64_t length) {
  image_repo_.addCustomImage(name, hash, length);
}

void UptaneRepo::signTargets() { director_repo_.signTargets(); }

void UptaneRepo::emptyTargets() { director_repo_.emptyTargets(); }
void UptaneRepo::oldTargets() { director_repo_.oldTargets(); }