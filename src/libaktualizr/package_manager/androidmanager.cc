#include <regex>

#include "androidmanager.h"

#include "utilities/utils.h"

Json::Value AndroidManager::getInstalledPackages() const {
  Json::Value packages(Json::arrayValue);
  Json::Value package;
  package["name"] = "aktualizr";
  package["version"] = "n/a";
  packages.append(package);
  return packages;
}

Uptane::Target AndroidManager::getCurrent() const {
  std::vector<Uptane::Target> installed_versions;
  std::string current_hash;
  size_t current_k = SIZE_MAX;
  storage_->loadPrimaryInstalledVersions(&installed_versions, &current_k, nullptr);
  if (current_k != SIZE_MAX) {
    current_hash = installed_versions[current_k].sha256Hash();
  }
  AndroidInstallationState installation_state;
  if (AndroidInstallationDispatcher::GetState().state_ == AndroidInstallationState::STATE_INSTALLED) {
    installation_state = AndroidInstallationDispatcher::Dispatch();
    if (installation_state.state_ == AndroidInstallationState::STATE_UPDATE) {
      current_hash = installation_state.payload_;
    }
  }
  std::vector<Uptane::Target>::iterator it;
  for (it = installed_versions.begin(); it != installed_versions.end(); it++) {
    if (it->sha256Hash() == current_hash) {
      if (installation_state.state_ == AndroidInstallationState::STATE_UPDATE) {
        storage_->savePrimaryInstalledVersion(*it, InstalledVersionUpdateMode::kCurrent);
        auto outcome = data::InstallOutcome(data::UpdateResultCode::kOk, "package successfully intalled");
        storage_->storeInstallationResult(data::OperationResult(it->filename(), outcome));
      }
      return *it;
    }
  }
  return getUnknown();
}

data::InstallOutcome AndroidManager::install(const Uptane::Target& target) const {
  if (AndroidInstallationDispatcher::GetState().state_ == AndroidInstallationState::STATE_NOP) {
    LOG_INFO << "Begin Android package installation";
    storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kPending);
    auto package_filename = (Utils::getStorageRootPath() / target.filename()).string() + "." + target.sha256Hash();
    std::ofstream package_file(package_filename.c_str());
    if (!package_file.good()) {
      throw std::runtime_error(std::string("Error opening file ") + package_filename);
    }
    package_file << *storage_->openTargetFile(target);
  }
  return data::InstallOutcome(data::UpdateResultCode::kInProgress, "Installation is in progress");
}

data::InstallOutcome AndroidManager::finalizeInstall(const Uptane::Target& target) const {
  (void)target;
  return data::InstallOutcome(data::UpdateResultCode::kOk, "package installation successfully finalized");
}

AndroidInstallationState AndroidInstallationDispatcher::GetState() {
  boost::filesystem::directory_iterator entryItEnd, entryIt(Utils::getStorageRootPath());
  for (; entryIt != entryItEnd; ++entryIt) {
    auto& entry_path = entryIt->path();
    if (boost::filesystem::is_directory(*entryIt)) {
      continue;
    }
    auto package_file_path = entry_path.string();
    auto ext = entry_path.extension().string();
    if (ext == ".installed") {
      return {AndroidInstallationState::STATE_INSTALLED, package_file_path};
    } else if (ext == ".inprogress") {
      return {AndroidInstallationState::STATE_IN_PROGRESS, package_file_path};
    } else if (std::regex_match(ext, std::regex("\\.[[:xdigit:]]+"))) {
      return {AndroidInstallationState::STATE_READY, package_file_path};
    }
  }
  return {AndroidInstallationState::STATE_NOP, ""};
}

AndroidInstallationState AndroidInstallationDispatcher::Dispatch() {
  auto current = GetState();
  if (current.state_ == AndroidInstallationState::STATE_INSTALLED) {
    auto hash = boost::filesystem::path(current.payload_).stem().extension().string();
    if (!hash.empty()) {
      hash = hash.substr(1);
    }
    boost::filesystem::remove(current.payload_);
    return {AndroidInstallationState::STATE_UPDATE, hash};
  } else if (current.state_ == AndroidInstallationState::STATE_IN_PROGRESS) {
    boost::filesystem::path installed_package_file_path(current.payload_);
    installed_package_file_path.replace_extension(".installed");
    boost::filesystem::rename(boost::filesystem::path(current.payload_), installed_package_file_path);
    return {AndroidInstallationState::STATE_UPDATE, installed_package_file_path.string()};
  } else if (std::regex_match(boost::filesystem::path(current.payload_).extension().string(),
                              std::regex("\\.[[:xdigit:]]+"))) {
    boost::filesystem::path inprogress_package_file_path = boost::filesystem::path(current.payload_ + ".inprogress");
    boost::filesystem::rename(boost::filesystem::path(current.payload_), inprogress_package_file_path);
    return {AndroidInstallationState::STATE_IN_PROGRESS, inprogress_package_file_path.string()};
  }
  return {AndroidInstallationState::STATE_NOP, ""};
}
