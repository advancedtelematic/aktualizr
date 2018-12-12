#include "opcuaserver_secondary_delegate.h"
#include "aktualizr_secondary_common.h"

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
    PublicKey pk = secondary_->keys_.UptanePublicKey();
    model->configuration_.setPublicKeyType(pk.Type());
    model->configuration_.setPublicKey(pk.Value());

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
  // TODO: parsing as a part of reception is WRONG
  if (metadata != Json::nullValue && metadata.isMember("signed")) {
    if (metadata["signed"]["_type"].asString() == "Root") {
      received_meta_pack_.director_root = json;
    } else if (metadata["signed"]["_type"].asString() == "Targets") {
      received_meta_pack_.director_targets = json;
    }
  }
}

void OpcuaServerSecondaryDelegate::handleAllMetaDataFilesReceived(opcuabridge::ServerModel* model) {
  TimeStamp now(TimeStamp::Now());
  secondary_->detected_attack_.clear();

  std::string root_str;
  secondary_->storage_->loadLatestRoot(&root_str, Uptane::RepositoryType::Director());
  secondary_->root_ = Uptane::Root(Uptane::RepositoryType::Director(), Utils::parseJSON(root_str));

  std::string targets_str;
  secondary_->storage_->loadNonRoot(&targets_str, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
  secondary_->meta_targets_ = Uptane::Targets(Utils::parseJSON(targets_str));

  try {
    // TODO: proper root metadata rotation
    secondary_->root_ = Uptane::Root(Uptane::RepositoryType::Director(),
                                     Utils::parseJSON(received_meta_pack_.director_root), secondary_->root_);
    Uptane::Targets targets(Uptane::RepositoryType::Director(), Utils::parseJSON(received_meta_pack_.director_targets),
                            secondary_->root_);
    if (secondary_->meta_targets_.version() > targets.version()) {
      secondary_->detected_attack_ = "Rollback attack detected";
      LOG_ERROR << "Uptane security check: " << secondary_->detected_attack_;
      return;
    }
    bool target_found = false;
    secondary_->meta_targets_ = Uptane::Targets(received_meta_pack_.director_targets);
    for (auto it = secondary_->meta_targets_.targets.begin(); it != secondary_->meta_targets_.targets.end(); ++it) {
      if (it->IsForSecondary(secondary_->ecu_serial_)) {
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
  secondary_->storage_->storeRoot(received_meta_pack_.director_root, Uptane::RepositoryType::Director(),
                                  Uptane::Version(secondary_->root_.version()));
  secondary_->storage_->storeNonRoot(received_meta_pack_.director_targets, Uptane::RepositoryType::Director(),
                                     Uptane::Role::Targets());
}

void OpcuaServerSecondaryDelegate::handleDirectoryFilesSynchronized(opcuabridge::ServerModel* model) {
  if (secondary_->target_) {
    auto target_to_install = *secondary_->target_;
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
      data::InstallationResult result = secondary_->pacman->install(target_to_install);
      secondary_->storage_->saveEcuInstallationResult(secondary_->ecu_serial_, result);
      if (result.result_code.num_code != data::ResultCode::Numeric::kOk) {
        LOG_ERROR << "Could not install target (" << result.result_code.toString() << "): " << result.description;
      } else {
        secondary_->storage_->saveInstalledVersion(secondary_->ecu_serial_.ToString(), target_to_install,
                                                   InstalledVersionUpdateMode::kCurrent);
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
