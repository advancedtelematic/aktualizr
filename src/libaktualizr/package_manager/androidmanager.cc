#include <boost/algorithm/string/find.hpp>
#include <boost/filesystem.hpp>
#include <regex>

#include "androidmanager.h"

#include "utilities/utils.h"

#include <boost/phoenix/stl/algorithm/transformation.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/qi.hpp>

namespace qi = boost::spirit::qi;
namespace fs = boost::filesystem;

const std::string AndroidManager::data_ota_package_dir_ = "/data/ota_package";

Json::Value AndroidManager::getInstalledPackages() const {
  using boost::phoenix::copy;
  using qi::_1;
  using qi::char_;

  std::string pm_output;
  Json::Value packages(Json::arrayValue);
  if (0 != Utils::shell("pm list packages --show-versioncode", &pm_output)) {
    return packages;
  }

  qi::rule<std::string::iterator, std::vector<char>()> char_seq = qi::lexeme[*(char_ - ' ')];

  std::istringstream pv_lines(pm_output);
  for (std::string line; std::getline(pv_lines, line);) {
    std::string p, v;
    if (qi::parse(line.begin(), line.end(),
                  ("package:" >> char_seq[copy(_1, std::back_inserter(p))] >> ' ' >> "versionCode:" >>
                   char_seq[copy(_1, std::back_inserter(v))]))) {
      Json::Value package;
      package["name"] = p;
      package["version"] = v;
      packages.append(package);
    }
  }
  return packages;
}

Uptane::Target AndroidManager::getCurrent() const {
  std::string getprop_output;
  std::smatch hash_match;
  if (0 != Utils::shell("getprop ota.last_installed_package_file", &getprop_output) ||
      !std::regex_search(getprop_output, hash_match, std::regex("\\.[[:xdigit:]]+"))) {
    return Uptane::Target::Unknown();
  }
  const std::string installed_package_hash = hash_match[0].str().substr(1);
  std::vector<Uptane::Target> installed_versions;
  storage_->loadPrimaryInstalledVersions(&installed_versions, nullptr, nullptr);
  for (const auto& target : installed_versions) {
    if (target.sha256Hash() == installed_package_hash) {
      return target;
    }
  }
  return Uptane::Target::Unknown();
}

data::InstallOutcome AndroidManager::install(const Uptane::Target& target) const {
  LOG_INFO << "Begin Android package installation";
  auto package_filename = (fs::path(data_ota_package_dir_) / target.filename()).string() + "." + target.sha256Hash();
  std::ofstream package_file(package_filename.c_str());
  if (!package_file.good()) {
    throw std::runtime_error(std::string("Error opening file ") + package_filename);
  }
  package_file << *storage_->openTargetFile(target);

  if (bootloader_ != nullptr) {
    bootloader_->rebootFlagSet();
  }
  return data::InstallOutcome(data::UpdateResultCode::kNeedCompletion, "need reboot");
}

data::InstallOutcome AndroidManager::finalizeInstall(const Uptane::Target& target) const {
  std::string ota_package_file_path = GetOTAPackageFilePath(target.sha256Hash());
  if (!ota_package_file_path.empty()) fs::remove(ota_package_file_path);
  std::string errorMessage{"n/a"};
  if (installationAborted(&errorMessage)) {
    return data::InstallOutcome(data::UpdateResultCode::kInstallFailed, errorMessage);
  }
  return data::InstallOutcome(data::UpdateResultCode::kOk, "package installation successfully finalized");
}

std::string AndroidManager::GetOTAPackageFilePath(const std::string& hash) {
  fs::directory_iterator entryItEnd, entryIt{fs::path(data_ota_package_dir_)};
  for (; entryIt != entryItEnd; ++entryIt) {
    auto& entry_path = entryIt->path();
    if (boost::filesystem::is_directory(*entryIt)) {
      continue;
    }
    auto ext = entry_path.extension().string();
    ext = ext.substr(1);
    if (ext == hash) {
      return entry_path.string();
    }
  }
  return std::string{};
}

bool AndroidManager::installationAborted(std::string* errorMessage) const {
  std::string installation_log_file{"/cache/recovery/last_install"};
  std::ifstream log(installation_log_file.c_str());
  if (!log.good()) {
    throw std::runtime_error(std::string("Error opening file ") + installation_log_file);
  }
  for (std::string line; std::getline(log, line);) {
    if (boost::algorithm::find_first(line, "error:")) {
      int error_code = std::stoi(line.substr(6));
      if (error_code != 0) {
        *errorMessage = std::string("Error code: ") + std::to_string(error_code);
        return true;
      }
    }
  }
  return false;
}
