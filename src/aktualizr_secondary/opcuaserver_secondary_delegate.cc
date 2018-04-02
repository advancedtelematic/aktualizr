#include "opcuaserver_secondary_delegate.h"
#include "aktualizr_secondary_common.h"

#include <logging.h>
#include <ostreereposync.h>
#include <utils.h>

OpcuaServerSecondaryDelegate::OpcuaServerSecondaryDelegate(AktualizrSecondaryCommon* secondary)
    : secondary_(secondary), ostree_sync_working_repo_dir_("opcuabridge-ostree-sync-working-repo") {}

void OpcuaServerSecondaryDelegate::handleServerInitialized(opcuabridge::ServerModel* model) {
  if (secondary_->uptaneInitialize()) {
    model->configuration_.setSerial(secondary_->ecu_serial_);
    model->configuration_.setHwId(secondary_->hardware_id_);
    model->configuration_.setPublicKeyType(secondary_->config_.uptane.key_type);
    model->configuration_.setPublicKey(secondary_->keys_.getUptanePublicKey());

    if (ostree_repo_sync::ArchiveModeRepo(secondary_->config_.pacman.sysroot)) {
      model->file_data_.setBasePath(secondary_->config_.pacman.sysroot);
    } else {
      model->file_data_.setBasePath(ostree_sync_working_repo_dir_.Path());
      if (!ostree_repo_sync::LocalPullRepo(secondary_->config_.pacman.sysroot, ostree_sync_working_repo_dir_.Path())) {
        LOG_ERROR << "OSTree repo sync failed: unable to local pull from " << secondary_->config_.pacman.sysroot;
        return;
      }
    }
    opcuabridge::UpdateFileList(&model->file_list_, model->file_data_.getBasePath());
  } else {
    LOG_ERROR << "Secondary: failed to initialize";
  }
}

void OpcuaServerSecondaryDelegate::handleVersionReportRequested(opcuabridge::ServerModel* model) {
  opcuabridge::ECUVersionManifest manifest;
  manifest.unwrapMessage(secondary_->keys_.signTuf(secondary_->pacman->getManifest(secondary_->ecu_serial_)));
  model->version_report_.setEcuVersionManifest(manifest);
  model->version_report_.getEcuVersionManifest().getEcuVersionManifestSigned().setSecurityAttack(
      secondary_->detected_attack_);
}

void OpcuaServerSecondaryDelegate::handleMetaDataFileReceived(opcuabridge::ServerModel* model) {
  std::string json(model->metadatafile_.getMetadata().begin(), model->metadatafile_.getMetadata().end());
  auto metadata = Utils::parseJSON(json);
  if (metadata != Json::nullValue && metadata.isMember("signed")) {
    if (metadata["signed"]["_type"].asString() == "Root") {
      received_meta_pack_.director_root = Uptane::Root("director", metadata);
    } else if (metadata["signed"]["_type"].asString() == "Targets") {
      received_meta_pack_.director_targets = Uptane::Targets(metadata);
    }
  }
}

void OpcuaServerSecondaryDelegate::handleAllMetaDataFilesReceived(opcuabridge::ServerModel* model) {
  Uptane::TimeStamp now(Uptane::TimeStamp::Now());
  secondary_->detected_attack_.clear();

  secondary_->root_ = Uptane::Root(now, "director", received_meta_pack_.director_root.original(), secondary_->root_);
  Uptane::Targets targets(now, "director", received_meta_pack_.director_targets.original(), secondary_->root_);
  if (secondary_->meta_targets_.version() > targets.version()) {
    secondary_->detected_attack_ = "Rollback attack detected";
    return;
  }
  secondary_->meta_targets_ = targets;
  std::vector<Uptane::Target>::const_iterator it;
  bool target_found = false;
  for (it = secondary_->meta_targets_.targets.begin(); it != secondary_->meta_targets_.targets.end(); ++it) {
    if (it->IsForSecondary(secondary_->ecu_serial_)) {
      if (target_found) {
        secondary_->detected_attack_ = "Duplicate entry for this ECU";
        break;
      }
      target_found = true;
      secondary_->target_ = std::unique_ptr<Uptane::Target>(new Uptane::Target(*it));
    }
  }
  secondary_->storage_->storeMetadata(received_meta_pack_);
}

void OpcuaServerSecondaryDelegate::handleDirectoryFilesSynchronized(opcuabridge::ServerModel* model) {
  if (!ostree_repo_sync::ArchiveModeRepo(secondary_->config_.pacman.sysroot)) {
    if (!ostree_repo_sync::LocalPullRepo(ostree_sync_working_repo_dir_.Path(), secondary_->config_.pacman.sysroot)) {
      LOG_ERROR << "OSTree repo sync failed: unable to local pull from "
                << ostree_sync_working_repo_dir_.Path().string();
      return;
    }
  }
  if (secondary_->target_ != nullptr) {
    data::UpdateResultCode res_code;
    std::string message;
    std::tie(res_code, message) = secondary_->pacman->install(*secondary_->target_);
    if (res_code != data::UpdateResultCode::OK)
      LOG_ERROR << "Could not install target (" << res_code << "): " << message;
  } else {
    LOG_ERROR << "No valid installation target found";
  }
}
