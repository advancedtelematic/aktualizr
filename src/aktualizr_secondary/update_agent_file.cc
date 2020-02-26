#include "update_agent_file.h"
#include "logging/logging.h"
#include "uptane/manifest.h"

bool FileUpdateAgent::isTargetSupported(const Uptane::Target& target) const { return target.type() != "OSTREE"; }

bool FileUpdateAgent::getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const {
  if (boost::filesystem::exists(target_filepath_)) {
    auto file_content = Utils::readFile(target_filepath_);

    installed_image_info.name = current_target_name_;
    installed_image_info.len = file_content.size();
    installed_image_info.hash = Uptane::ManifestIssuer::generateVersionHashStr(file_content);
  } else {
    // mimic the Primary's fake package manager behavior
    auto unknown_target = Uptane::Target::Unknown();
    installed_image_info.name = unknown_target.filename();
    installed_image_info.len = unknown_target.length();
    installed_image_info.hash = unknown_target.sha256Hash();
  }

  return true;
}

bool FileUpdateAgent::download(const Uptane::Target& target, const std::string& data) {
  auto target_hashes = target.hashes();
  if (target_hashes.size() == 0) {
    LOG_ERROR << "No hash found in the target metadata: " << target.filename();
    return false;
  }

  try {
    auto received_image_data_hash = Uptane::ManifestIssuer::generateVersionHash(data);

    if (!target.MatchHash(received_image_data_hash)) {
      LOG_ERROR << "The received image data hash doesn't match the hash specified in the target metadata,"
                   " hash type: "
                << target_hashes[0].TypeString();
      return false;
    }

    Utils::writeFile(target_filepath_, data);
    current_target_name_ = target.filename();

  } catch (const std::exception& exc) {
    LOG_ERROR << "Failed to generate a hash of the received image data: " << exc.what();
    return false;
  }
  return true;
}

data::ResultCode::Numeric FileUpdateAgent::install(const Uptane::Target& target) {
  (void)target;
  return data::ResultCode::Numeric::kOk;
}

void FileUpdateAgent::completeInstall() {}

data::InstallationResult FileUpdateAgent::applyPendingInstall(const Uptane::Target& target) {
  (void)target;
  return data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                  "Applying of the pending updates are not supported by the file update agent");
}
