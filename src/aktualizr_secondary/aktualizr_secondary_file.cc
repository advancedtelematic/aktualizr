#include "aktualizr_secondary_file.h"
#include "update_agent_file.h"

const std::string AktualizrSecondaryFile::FileUpdateDefaultFile{"firmware.txt"};

AktualizrSecondaryFile::AktualizrSecondaryFile(const AktualizrSecondaryConfig& config)
    : AktualizrSecondaryFile(config, INvStorage::newStorage(config.storage)) {}

AktualizrSecondaryFile::AktualizrSecondaryFile(const AktualizrSecondaryConfig& config,
                                               std::shared_ptr<INvStorage> storage,
                                               std::shared_ptr<FileUpdateAgent> update_agent)
    : AktualizrSecondary(config, std::move(storage)), update_agent_{std::move(update_agent)} {
  registerHandler(AKIpUptaneMes_PR_uploadDataReq, std::bind(&AktualizrSecondaryFile::uploadDataHdlr, this,
                                                            std::placeholders::_1, std::placeholders::_2));
  if (!update_agent_) {
    std::string current_target_name;

    boost::optional<Uptane::Target> current_version;
    boost::optional<Uptane::Target> pending_version;
    auto installed_version_res =
        AktualizrSecondary::storage().loadInstalledVersions("", &current_version, &pending_version);

    if (installed_version_res && !!current_version) {
      current_target_name = current_version->filename();
    } else {
      current_target_name = "unknown";
    }

    update_agent_ = std::make_shared<FileUpdateAgent>(config.storage.path / FileUpdateDefaultFile, current_target_name);
  }
}

void AktualizrSecondaryFile::initialize() { initPendingTargetIfAny(); }

data::ResultCode::Numeric AktualizrSecondaryFile::receiveData(const uint8_t* data, size_t size) {
  if (!pendingTarget().IsValid()) {
    LOG_ERROR << "Aborting image download/receiving; no valid target found.";
    return data::ResultCode::Numeric::kGeneralError;
  }

  LOG_INFO << "Receiving target image data from Primary: " << size;
  return update_agent_->receiveData(pendingTarget(), data, size);
}

bool AktualizrSecondaryFile::isTargetSupported(const Uptane::Target& target) const {
  return update_agent_->isTargetSupported(target);
}

data::InstallationResult AktualizrSecondaryFile::applyPendingInstall(const Uptane::Target& target) {
  return update_agent_->applyPendingInstall(target);
}

bool AktualizrSecondaryFile::getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const {
  return update_agent_->getInstalledImageInfo(installed_image_info);
}

data::ResultCode::Numeric AktualizrSecondaryFile::installPendingTarget(const Uptane::Target& target) {
  return update_agent_->install(target);
}

void AktualizrSecondaryFile::completeInstall() { return update_agent_->completeInstall(); }

MsgHandler::ReturnCode AktualizrSecondaryFile::uploadDataHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
  LOG_INFO << "Handling upload data request from Primary";

  auto rec_buf_size = in_msg.uploadDataReq()->data.size;
  if (rec_buf_size < 0) {
    LOG_ERROR << "The received data buffer size has a negative size: " << rec_buf_size;
    return ReturnCode::kOk;
  }

  auto send_firmware_result = receiveData(in_msg.uploadDataReq()->data.buf, static_cast<size_t>(rec_buf_size));

  out_msg.present(AKIpUptaneMes_PR_uploadDataResp).uploadDataResp()->result =
      (send_firmware_result == data::ResultCode::Numeric::kOk) ? AKInstallationResult_success
                                                               : AKInstallationResult_failure;

  return ReturnCode::kOk;
}
