#include "update_agent_file.h"
#include <fstream>
#include "crypto/crypto.h"
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

data::ResultCode::Numeric FileUpdateAgent::install(const Uptane::Target& target) {
  if (!boost::filesystem::exists(new_target_filepath_)) {
    LOG_ERROR << "The target image has not been received";
    return data::ResultCode::Numeric::kDownloadFailed;
  }

  auto received_target_image_size = boost::filesystem::file_size(new_target_filepath_);
  if (received_target_image_size != target.length()) {
    LOG_ERROR << "Received target image size does not match the size specified in metadata: "
              << received_target_image_size << " != " << target.length();
    boost::filesystem::remove(new_target_filepath_);
    return data::ResultCode::Numeric::kDownloadFailed;
  }

  if (!target.MatchHash(new_target_hasher_->getHash())) {
    LOG_ERROR << "The received target image hash does not match the hash specified in metadata: "
              << new_target_hasher_->getHash() << " != " << getTargetHash(target).HashString();
    return data::ResultCode::Numeric::kDownloadFailed;
  }

  boost::filesystem::rename(new_target_filepath_, target_filepath_);

  if (boost::filesystem::exists(new_target_filepath_)) {
    return data::ResultCode::Numeric::kInstallFailed;
  }

  if (!boost::filesystem::exists(target_filepath_)) {
    LOG_ERROR << "The target image has not been installed";
    return data::ResultCode::Numeric::kInstallFailed;
  }

  current_target_name_ = target.filename();
  new_target_hasher_.reset();
  return data::ResultCode::Numeric::kOk;
}

void FileUpdateAgent::completeInstall() {}

data::InstallationResult FileUpdateAgent::applyPendingInstall(const Uptane::Target& target) {
  (void)target;
  return data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                  "Applying of the pending updates are not supported by the file update agent");
}

data::ResultCode::Numeric FileUpdateAgent::receiveData(const Uptane::Target& target, const uint8_t* data, size_t size) {
  std::ofstream target_file(new_target_filepath_.c_str(),
                            std::ofstream::out | std::ofstream::binary | std::ofstream::app);

  if (!target_file.good()) {
    LOG_ERROR << "Failed to open a new target image file";
    return data::ResultCode::Numeric::kDownloadFailed;
  }

  auto current_new_image_size = target_file.tellp();
  if (-1 == current_new_image_size) {
    LOG_ERROR << "Failed to obtain a size of the new target image that is being uploaded";
    target_file.close();
    return data::ResultCode::Numeric::kDownloadFailed;
  }

  if (static_cast<uint64_t>(current_new_image_size) >= target.length()) {
    LOG_ERROR << "The size of the received new target image data exceeds the target image size: "
              << current_new_image_size << " != " << target.length();
    target_file.close();
    return data::ResultCode::Numeric::kDownloadFailed;
  }

  if (current_new_image_size == 0) {
    new_target_hasher_ = MultiPartHasher::create(getTargetHash(target).type());
  }

  target_file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  auto written_data_size = target_file.tellp() - current_new_image_size;

  if (written_data_size < 0 || static_cast<size_t>(written_data_size) != size) {
    LOG_ERROR << "The amount of written data is not equal to amount of received data: " << written_data_size
              << " != " << size;
    target_file.close();
    return data::ResultCode::Numeric::kDownloadFailed;
  }

  target_file.close();

  LOG_INFO << "Received and stored data of a new target image;"
              " received in this request (bytes): "
           << size << " total received so far: " << (current_new_image_size + written_data_size)
           << " expected total: " << target.length();

  new_target_hasher_->update(data, size);

  return data::ResultCode::Numeric::kOk;
}

Hash FileUpdateAgent::getTargetHash(const Uptane::Target& target) {
  // TODO(OTA-4831): check target.hashes() size.
  return target.hashes()[0];
}
