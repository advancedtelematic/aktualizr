#include "update_agent_file.h"
#include <fstream>
#include "crypto/crypto.h"
#include "logging/logging.h"
#include "uptane/manifest.h"

// TODO(OTA-4939): Unify this with the check in
// SotaUptaneClient::getNewTargets() and make it more generic.
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

data::InstallationResult FileUpdateAgent::install(const Uptane::Target& target) {
  if (!boost::filesystem::exists(new_target_filepath_)) {
    LOG_ERROR << "The target image has not been received";
    return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed,
                                    "The target image has not been received");
  }

  auto received_target_image_size = boost::filesystem::file_size(new_target_filepath_);
  if (received_target_image_size != target.length()) {
    LOG_ERROR << "Received image size does not match the size specified in Target metadata: "
              << received_target_image_size << " != " << target.length();
    boost::filesystem::remove(new_target_filepath_);
    return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed,
                                    "Received image size does not match the size specified in Target metadata: " +
                                        std::to_string(received_target_image_size) +
                                        " != " + std::to_string(target.length()));
  }

  if (!target.MatchHash(new_target_hasher_->getHash())) {
    LOG_ERROR << "The received image's hash does not match the hash specified in Target metadata: "
              << new_target_hasher_->getHash() << " != " << getTargetHash(target).HashString();
    return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed,
                                    "The received image's hash does not match the hash specified in Target metadata: " +
                                        new_target_hasher_->getHash().HashString() +
                                        " != " + getTargetHash(target).HashString());
  }

  boost::filesystem::rename(new_target_filepath_, target_filepath_);

  if (boost::filesystem::exists(new_target_filepath_)) {
    return data::InstallationResult(data::ResultCode::Numeric::kInstallFailed,
                                    "The target image has not been installed");
  }

  if (!boost::filesystem::exists(target_filepath_)) {
    return data::InstallationResult(data::ResultCode::Numeric::kInstallFailed,
                                    "The target image has not been installed");
  }

  current_target_name_ = target.filename();
  new_target_hasher_.reset();
  return data::InstallationResult(data::ResultCode::Numeric::kOk, "");
}

void FileUpdateAgent::completeInstall() {}

data::InstallationResult FileUpdateAgent::applyPendingInstall(const Uptane::Target& target) {
  (void)target;
  return data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                  "Applying pending updates is not supported by the file update agent");
}

data::InstallationResult FileUpdateAgent::receiveData(const Uptane::Target& target, const uint8_t* data, size_t size) {
  std::ofstream target_file(new_target_filepath_.c_str(),
                            std::ofstream::out | std::ofstream::binary | std::ofstream::app);

  if (!target_file.good()) {
    LOG_ERROR << "Failed to open a new target image file";
    return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed,
                                    "Failed to open a new target image file");
  }

  auto current_new_image_size = target_file.tellp();
  if (-1 == current_new_image_size) {
    LOG_ERROR << "Failed to obtain a size of the new target image that is being uploaded";
    target_file.close();
    return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed,
                                    "Failed to obtain a size of the new target image that is being uploaded");
  }

  if (static_cast<uint64_t>(current_new_image_size) >= target.length()) {
    LOG_ERROR << "The size of the received image data exceeds the expected Target image size: "
              << current_new_image_size << " != " << target.length();
    target_file.close();
    return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed,
                                    "The size of the received image data exceeds the expected Target image size: " +
                                        std::to_string(current_new_image_size) +
                                        " != " + std::to_string(target.length()));
  }

  if (current_new_image_size == 0) {
    new_target_hasher_ = MultiPartHasher::create(getTargetHash(target).type());
  }

  target_file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  auto written_data_size = target_file.tellp() - current_new_image_size;

  if (written_data_size < 0 || static_cast<size_t>(written_data_size) != size) {
    LOG_ERROR << "The size of data written is not equal to the received data size: " << written_data_size
              << " != " << size;
    target_file.close();
    return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed,
                                    "The size of data written is not equal to the received data size: " +
                                        std::to_string(written_data_size) + " != " + std::to_string(size));
  }

  target_file.close();

  LOG_INFO << "Received and stored data of a new target image;"
              " received in this request (bytes): "
           << size << " total received so far: " << (current_new_image_size + written_data_size)
           << " expected total: " << target.length();

  new_target_hasher_->update(data, size);

  return data::InstallationResult(data::ResultCode::Numeric::kOk, "");
}

Hash FileUpdateAgent::getTargetHash(const Uptane::Target& target) {
  // TODO(OTA-4831): check target.hashes() size.
  return target.hashes()[0];
}
