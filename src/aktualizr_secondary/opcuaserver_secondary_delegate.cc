#include "opcuaserver_secondary_delegate.h"
#include "aktualizr_secondary/aktualizr_secondary_common.h"

#include "logging/logging.h"
#include "package_manager/ostreereposync.h"
#include "package_manager/packagemanagerinterface.h"
#include "utilities/utils.h"

#include <thread>

#include <memory>

namespace fs = boost::filesystem;

OpcuaServerSecondaryDelegate::OpcuaServerSecondaryDelegate(AktualizrSecondaryCommon* secondary)
    : secondary_(secondary), ostree_sync_working_repo_dir_("opcuabridge-ostree-sync-working-repo") {}

void OpcuaServerSecondaryDelegate::handleServerInitialized(opcuabridge::ServerModel* model) {
  if (secondary_->uptaneInitialize()) {
    model->configuration_.setSerial(secondary_->ecu_serial_);
    model->configuration_.setHwId(secondary_->hardware_id_);
    model->configuration_.setPublicKeyType(secondary_->config_.uptane.key_type);
    model->configuration_.setPublicKey(secondary_->keys_.getUptanePublicKey());

    fs::path ostree_source_repo = ostree_repo_sync::GetOstreeRepoPath(secondary_->config_.pacman.sysroot);

    if (ostree_repo_sync::ArchiveModeRepo(ostree_source_repo)) {
      model->file_data_.setBasePath(ostree_source_repo);
    } else {
      model->file_data_.setBasePath(ostree_sync_working_repo_dir_.Path());
      if (!ostree_repo_sync::LocalPullRepo(ostree_source_repo, ostree_sync_working_repo_dir_.Path())) {
        LOG_ERROR << "OSTree repo sync failed: unable to local pull from " << ostree_source_repo.string();
      }
    }
  } else {
    LOG_ERROR << "Secondary: failed to initialize";
  }
}

void OpcuaServerSecondaryDelegate::handleVersionReportRequested(opcuabridge::ServerModel* model) {}

void OpcuaServerSecondaryDelegate::handleOriginalManifestRequested(opcuabridge::ServerModel* model) {
  auto manifest_signed =
      Utils::jsonToStr(secondary_->keys_.signTuf(secondary_->pacman->getManifest(secondary_->ecu_serial_)));
  model->original_manifest_.getBlock().resize(manifest_signed.size());
  std::copy(manifest_signed.begin(), manifest_signed.end(), model->original_manifest_.getBlock().begin());
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

  secondary_->root_ = Uptane::Root(Uptane::Root::kAcceptAll);
  Uptane::MetaPack meta;
  if (secondary_->storage_->loadMetadata(&meta)) {
    secondary_->root_ = meta.director_root;
    secondary_->meta_targets_ = meta.director_targets;
  }

  try {
    secondary_->root_ = Uptane::Root(now, "director", received_meta_pack_.director_root.original(), secondary_->root_);
    Uptane::Targets targets(now, "director", received_meta_pack_.director_targets.original(), secondary_->root_);
    if (secondary_->meta_targets_.version() > targets.version()) {
      secondary_->detected_attack_ = "Rollback attack detected";
      LOG_ERROR << "Uptane security check: " << secondary_->detected_attack_;
      return;
    }
    bool target_found = false;
    secondary_->meta_targets_ = targets;
    for (auto it = secondary_->meta_targets_.targets.begin(); it != secondary_->meta_targets_.targets.end(); ++it) {
      if (it->ecu_identifier() == secondary_->ecu_serial_) {
        if (target_found) {
          secondary_->detected_attack_ = "Duplicate entry for this ECU";
          break;
        }
        target_found = true;
        secondary_->target_ = std_::make_unique<Uptane::Target>(*it);
      }
    }
  } catch (const Uptane::SecurityException& ex) {
    LOG_ERROR << "Uptane security check: " << ex.what();
    return;
  }
  secondary_->storage_->storeMetadata(received_meta_pack_);
}

void OpcuaServerSecondaryDelegate::handleDirectoryFilesSynchronized(opcuabridge::ServerModel* model) {
  if (secondary_->target_) {
    auto target_to_install = *secondary_->target_;
    secondary_->pacman->setOperationResult(target_to_install.filename(), data::IN_PROGRESS, "Installation in progress");
    std::thread long_run_op([=]() {
      fs::path ostree_source_repo = ostree_repo_sync::GetOstreeRepoPath(secondary_->config_.pacman.sysroot);
      if (!ostree_repo_sync::ArchiveModeRepo(ostree_source_repo)) {
        if (!ostree_repo_sync::LocalPullRepo(ostree_sync_working_repo_dir_.Path(), ostree_source_repo,
                                             target_to_install.sha256Hash())) {
          LOG_ERROR << "OSTree repo sync failed: unable to local pull from "
                    << ostree_sync_working_repo_dir_.Path().string();
          return;
        }
      }
      data::UpdateResultCode res_code;
      std::string message;
      std::tie(res_code, message) = secondary_->pacman->install(target_to_install);
      if (res_code != data::UpdateResultCode::OK) {
        LOG_ERROR << "Could not install target (" << res_code << "): " << message;
        secondary_->pacman->setOperationResult(target_to_install.filename(), res_code, message);
      } else {
        secondary_->storage_->saveInstalledVersion(target_to_install);
        secondary_->pacman->setOperationResult(target_to_install.filename(), data::OK, "Installation successful");
      }
    });
    long_run_op.detach();
  } else {
    LOG_ERROR << "No valid installation target found";
  }
}

void OpcuaServerSecondaryDelegate::handleDirectoryFileListRequested(opcuabridge::ServerModel* model) {
  opcuabridge::UpdateFileList(&model->file_list_, model->file_data_.getBasePath());
}
