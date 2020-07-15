#include "aktualizr_secondary_ostree.h"
#include "package_manager/ostreemanager.h"
#include "update_agent_ostree.h"

AktualizrSecondaryOstree::AktualizrSecondaryOstree(const AktualizrSecondaryConfig& config)
    : AktualizrSecondaryOstree(config, INvStorage::newStorage(config.storage)) {}

AktualizrSecondaryOstree::AktualizrSecondaryOstree(const AktualizrSecondaryConfig& config,
                                                   const std::shared_ptr<INvStorage>& storage)
    : AktualizrSecondary(config, storage) {
  registerHandler(AKIpUptaneMes_PR_downloadOstreeRevReq, std::bind(&AktualizrSecondaryOstree::downloadOstreeRev, this,
                                                                   std::placeholders::_1, std::placeholders::_2));

  std::shared_ptr<OstreeManager> pack_man =
      std::make_shared<OstreeManager>(config.pacman, config.bootloader, AktualizrSecondary::storagePtr(), nullptr);
  update_agent_ =
      std::make_shared<OstreeUpdateAgent>(config.pacman.sysroot, keyMngr(), pack_man, config.uptane.ecu_hardware_id);
}

void AktualizrSecondaryOstree::initialize() {
  initPendingTargetIfAny();

  if (hasPendingUpdate()) {
    LOG_INFO << "Found a pending target to be applied.";
    // TODO(OTA-4545): refactor this to make it simpler as we don't need to persist/store
    // an installation status of each ECU but store it just for a given secondary ECU
    std::vector<Uptane::Target> installed_versions;
    boost::optional<Uptane::Target> pending_target;
    AktualizrSecondary::storage().loadInstalledVersions(serial().ToString(), nullptr, &pending_target);

    if (!!pending_target) {
      data::InstallationResult install_res =
          data::InstallationResult(data::ResultCode::Numeric::kUnknown, "Unknown installation error");
      LOG_INFO << "Pending update found; attempting to apply it. Target hash: " << pending_target->sha256Hash();

      install_res = applyPendingInstall(*pending_target);

      if (install_res.result_code != data::ResultCode::Numeric::kNeedCompletion) {
        AktualizrSecondary::storage().saveEcuInstallationResult(serial(), install_res);

        if (install_res.isSuccess()) {
          LOG_INFO << "Pending update has been successfully applied: " << pending_target->sha256Hash();
          AktualizrSecondary::storage().saveInstalledVersion(serial().ToString(), *pending_target,
                                                             InstalledVersionUpdateMode::kCurrent);
        } else {
          LOG_ERROR << "Application of the pending update has failed: (" << install_res.result_code.toString() << ")"
                    << install_res.description;
          AktualizrSecondary::storage().saveInstalledVersion(serial().ToString(), *pending_target,
                                                             InstalledVersionUpdateMode::kNone);
        }

        directorRepo().dropTargets(AktualizrSecondary::storage());
      } else {
        LOG_INFO << "Pending update hasn't been applied because a reboot hasn't been detected";
      }
    }
  }
}

MsgHandler::ReturnCode AktualizrSecondaryOstree::downloadOstreeRev(Asn1Message& in_msg, Asn1Message& out_msg) {
  LOG_INFO << "Received an OSTree download request; attempting download...";
  auto result = downloadOstreeUpdate(ToString(in_msg.downloadOstreeRevReq()->tlsCred));

  auto m = out_msg.present(AKIpUptaneMes_PR_downloadOstreeRevResp).downloadOstreeRevResp();
  m->result = static_cast<AKInstallationResultCode_t>(result.result_code.num_code);
  SetString(&m->description, result.description);

  return ReturnCode::kOk;
}

data::InstallationResult AktualizrSecondaryOstree::downloadOstreeUpdate(const std::string& packed_tls_creds) {
  if (!pendingTarget().IsValid()) {
    LOG_ERROR << "Aborting image download; no valid target found.";
    return data::InstallationResult(data::ResultCode::Numeric::kGeneralError,
                                    "Aborting image download; no valid target found.");
  }

  auto result = update_agent_->downloadTargetRev(pendingTarget(), packed_tls_creds);
  if (!result.isSuccess()) {
    pendingTarget() = Uptane::Target::Unknown();
  }
  return result;
}

bool AktualizrSecondaryOstree::isTargetSupported(const Uptane::Target& target) const {
  return update_agent_->isTargetSupported(target);
}

data::InstallationResult AktualizrSecondaryOstree::applyPendingInstall(const Uptane::Target& target) {
  return update_agent_->applyPendingInstall(target);
}

bool AktualizrSecondaryOstree::getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const {
  return update_agent_->getInstalledImageInfo(installed_image_info);
}

data::InstallationResult AktualizrSecondaryOstree::installPendingTarget(const Uptane::Target& target) {
  return update_agent_->install(target);
}

void AktualizrSecondaryOstree::completeInstall() { return update_agent_->completeInstall(); }
