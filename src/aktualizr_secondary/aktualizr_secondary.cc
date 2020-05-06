#include "aktualizr_secondary.h"

#include "crypto/keymanager.h"
#include "logging/logging.h"
#include "update_agent.h"
#include "uptane/manifest.h"
#include "utilities/utils.h"

#include <sys/types.h>
#include <memory>

AktualizrSecondary::AktualizrSecondary(const AktualizrSecondaryConfig& config, std::shared_ptr<INvStorage> storage)
    : config_(config),
      storage_(std::move(storage)),
      keys_(std::make_shared<KeyManager>(storage_, config_.keymanagerConfig())) {
  uptaneInitialize();
  manifest_issuer_ = std::make_shared<Uptane::ManifestIssuer>(keys_, ecu_serial_);
  registerHandlers();
}

PublicKey AktualizrSecondary::publicKey() const { return keys_->UptanePublicKey(); }

Uptane::Manifest AktualizrSecondary::getManifest() const {
  Uptane::InstalledImageInfo installed_image_info;
  Uptane::Manifest manifest;

  if (getInstalledImageInfo(installed_image_info)) {
    manifest = manifest_issuer_->assembleAndSignManifest(installed_image_info);
  }

  return manifest;
}

bool AktualizrSecondary::putMetadata(const Metadata& metadata) { return doFullVerification(metadata); }

data::ResultCode::Numeric AktualizrSecondary::install() {
  if (!pending_target_.IsValid()) {
    LOG_ERROR << "Aborting target image installation; no valid target found.";
    return data::ResultCode::Numeric::kInternalError;
  }

  auto target_name = pending_target_.filename();
  auto install_result = installPendingTarget(pending_target_);

  switch (install_result) {
    case data::ResultCode::Numeric::kOk: {
      storage_->saveInstalledVersion(ecu_serial_.ToString(), pending_target_, InstalledVersionUpdateMode::kCurrent);
      pending_target_ = Uptane::Target::Unknown();
      LOG_INFO << "The target has been successfully installed: " << target_name;
      break;
    }
    case data::ResultCode::Numeric::kNeedCompletion: {
      storage_->saveInstalledVersion(ecu_serial_.ToString(), pending_target_, InstalledVersionUpdateMode::kPending);
      LOG_INFO << "The target has been successfully installed, but a reboot is required to be applied: " << target_name;
      break;
    }
    default: {
      LOG_INFO << "Failed to install the target: " << target_name;
    }
  }

  return install_result;
}

bool AktualizrSecondary::doFullVerification(const Metadata& metadata) {
  // 5.4.4.2. Full verification  https://uptane.github.io/uptane-standard/uptane-standard.html#metadata_verification

  // 1. Load and verify the current time or the most recent securely attested time.
  //    We trust the time that the given system/ECU provides.
  TimeStamp now(TimeStamp::Now());

  // 2. Download and check the Root metadata file from the Director repository.
  // 3. NOT SUPPORTED: Download and check the Timestamp metadata file from the Director repository.
  // 4. NOT SUPPORTED: Download and check the Snapshot metadata file from the Director repository.
  // 5. Download and check the Targets metadata file from the Director repository.
  try {
    director_repo_.updateMeta(*storage_, metadata);
  } catch (const std::exception& e) {
    LOG_ERROR << "Failed to update Director metadata: " << e.what();
    return false;
  }

  // 6. Download and check the Root metadata file from the Image repository.
  // 7. Download and check the Timestamp metadata file from the Image repository.
  // 8. Download and check the Snapshot metadata file from the Image repository.
  // 9. Download and check the top-level Targets metadata file from the Image repository.
  try {
    image_repo_.updateMeta(*storage_, metadata);
  } catch (const std::exception& e) {
    LOG_ERROR << "Failed to update Image repo metadata: " << e.what();
    return false;
  }

  // 10. Verify that Targets metadata from the Director and Image repositories match.
  if (!director_repo_.matchTargetsWithImageTargets(*(image_repo_.getTargets()))) {
    LOG_ERROR << "Targets metadata from the Director and Image repositories DOES NOT match ";
    return false;
  }

  auto targetsForThisEcu = director_repo_.getTargets(serial(), hwID());

  if (targetsForThisEcu.size() != 1) {
    LOG_ERROR << "Invalid number of targets (should be 1): " << targetsForThisEcu.size();
    return false;
  }

  if (!isTargetSupported(targetsForThisEcu[0])) {
    LOG_ERROR << "The given target type is not supported: " << targetsForThisEcu[0].type();
    return false;
  }

  pending_target_ = targetsForThisEcu[0];

  LOG_DEBUG << "Metadata verified, new update found.";
  return true;
}

void AktualizrSecondary::uptaneInitialize() {
  if (keys_->generateUptaneKeyPair().size() == 0) {
    throw std::runtime_error("Failed to generate Uptane key pair");
  }

  // from uptane/initialize.cc but we only take care of our own serial/hwid
  EcuSerials ecu_serials;

  if (storage_->loadEcuSerials(&ecu_serials)) {
    ecu_serial_ = ecu_serials[0].first;
    hardware_id_ = ecu_serials[0].second;
    return;
  }

  std::string ecu_serial_local = config_.uptane.ecu_serial;
  if (ecu_serial_local.empty()) {
    ecu_serial_local = keys_->UptanePublicKey().KeyId();
  }

  std::string ecu_hardware_id = config_.uptane.ecu_hardware_id;
  if (ecu_hardware_id.empty()) {
    ecu_hardware_id = Utils::getHostname();
    if (ecu_hardware_id == "") {
      throw std::runtime_error("Failed to define ECU hardware ID");
    }
  }

  ecu_serials.emplace_back(Uptane::EcuSerial(ecu_serial_local), Uptane::HardwareIdentifier(ecu_hardware_id));
  storage_->storeEcuSerials(ecu_serials);
  ecu_serial_ = ecu_serials[0].first;
  hardware_id_ = ecu_serials[0].second;

  // this is a way to find out and store a value of the target name that is installed
  // at the initial/provisioning stage and included into a device manifest
  // i.e. 'filepath' field or ["signed"]["installed_image"]["filepath"]
  // this value must match the value pushed to the backend during the bitbaking process,
  // specifically, at its OSTree push phase and is equal to
  // GARAGE_TARGET_NAME ?= "${OSTREE_BRANCHNAME}" which in turn is equal to OSTREE_BRANCHNAME ?= "${SOTA_HARDWARE_ID}"
  // therefore, by default GARAGE_TARGET_NAME == OSTREE_BRANCHNAME == SOTA_HARDWARE_ID
  // If there is no match then the backend/UI will not render/highlight currently installed version at all/correctly
  storage_->importInstalledVersions(config_.import.base_path);
}

void AktualizrSecondary::initPendingTargetIfAny() {
  try {
    director_repo_.checkMetaOffline(*storage_);
  } catch (const std::exception& e) {
    LOG_INFO << "No valid metadata found in storage.";
    return;
  }

  auto targetsForThisEcu = director_repo_.getTargets(ecu_serial_, hardware_id_);

  if (targetsForThisEcu.size() != 1) {
    LOG_ERROR << "Invalid number of targets (should be 1): " << targetsForThisEcu.size();
    return;
  }

  if (!isTargetSupported(targetsForThisEcu[0])) {
    LOG_ERROR << "The given target type is not supported: " << targetsForThisEcu[0].type();
    return;
  }

  pending_target_ = targetsForThisEcu[0];
}

void AktualizrSecondary::registerHandlers() {
  registerHandler(AKIpUptaneMes_PR_getInfoReq,
                  std::bind(&AktualizrSecondary::getInfoHdlr, this, std::placeholders::_1, std::placeholders::_2));

  registerHandler(AKIpUptaneMes_PR_manifestReq,
                  std::bind(&AktualizrSecondary::getManifestHdlr, this, std::placeholders::_1, std::placeholders::_2));

  registerHandler(AKIpUptaneMes_PR_putMetaReq2,
                  std::bind(&AktualizrSecondary::putMetaHdlr, this, std::placeholders::_1, std::placeholders::_2));

  registerHandler(AKIpUptaneMes_PR_installReq,
                  std::bind(&AktualizrSecondary::installHdlr, this, std::placeholders::_1, std::placeholders::_2));
}

MsgHandler::ReturnCode AktualizrSecondary::getInfoHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
  (void)in_msg;

  out_msg.present(AKIpUptaneMes_PR_getInfoResp);

  auto info_resp = out_msg.getInfoResp();

  SetString(&info_resp->ecuSerial, serial().ToString());
  SetString(&info_resp->hwId, hwID().ToString());
  info_resp->keyType = static_cast<AKIpUptaneKeyType_t>(publicKey().Type());
  SetString(&info_resp->key, publicKey().Value());

  return ReturnCode::kOk;
}

AktualizrSecondary::ReturnCode AktualizrSecondary::getManifestHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
  (void)in_msg;

  out_msg.present(AKIpUptaneMes_PR_manifestResp);
  auto manifest_resp = out_msg.manifestResp();
  manifest_resp->manifest.present = manifest_PR_json;
  SetString(&manifest_resp->manifest.choice.json, Utils::jsonToStr(getManifest()));  // NOLINT

  LOG_TRACE << "Manifest: \n" << getManifest();
  return ReturnCode::kOk;
}

AktualizrSecondary::ReturnCode AktualizrSecondary::putMetaHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
  auto md = in_msg.putMetaReq2();
  Uptane::RawMetaPack meta_pack;

  // TODO: what happens if something expected is missing? Or if something
  // appears twice?
  if (md->directorRepo.present == directorRepo_PR_collection) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    const int director_meta_count = md->directorRepo.choice.collection.list.count;
    for (int i = 0; i < director_meta_count; i++) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-union-access)
      const AKMetaJson_t object = *md->directorRepo.choice.collection.list.array[i];
      const std::string role = ToString(object.role);
      std::string json = ToString(object.json);
      if (role == Uptane::Role::ROOT) {
        meta_pack.director_root = std::move(json);
        LOG_DEBUG << "Received Director repo Root metadata:\n" << meta_pack.director_root;
      } else if (role == Uptane::Role::TARGETS) {
        meta_pack.director_targets = std::move(json);
        LOG_DEBUG << "Received Director repo Targets metadata:\n" << meta_pack.director_targets;
      } else {
        LOG_WARNING << "Director metadata in unknown format:" << md->directorRepo.present;
      }
    }
  }

  if (md->imageRepo.present == imageRepo_PR_collection) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    const int image_meta_count = md->imageRepo.choice.collection.list.count;
    for (int i = 0; i < image_meta_count; i++) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-union-access)
      const AKMetaJson_t object = *md->imageRepo.choice.collection.list.array[i];
      const std::string role = ToString(object.role);
      std::string json = ToString(object.json);
      if (role == Uptane::Role::ROOT) {
        meta_pack.image_root = std::move(json);
        LOG_DEBUG << "Received Image repo Root metadata:\n" << meta_pack.image_root;
      } else if (role == Uptane::Role::TIMESTAMP) {
        meta_pack.image_timestamp = std::move(json);
        LOG_DEBUG << "Received Image repo Timestamp metadata:\n" << meta_pack.image_timestamp;
      } else if (role == Uptane::Role::SNAPSHOT) {
        meta_pack.image_snapshot = std::move(json);
        LOG_DEBUG << "Received Image repo Snapshot metadata:\n" << meta_pack.image_snapshot;
      } else if (role == Uptane::Role::TARGETS) {
        meta_pack.image_targets = std::move(json);
        LOG_DEBUG << "Received Image repo Targets metadata:\n" << meta_pack.image_targets;
      } else {
        LOG_WARNING << "Image metadata in unknown format:" << md->imageRepo.present;
      }
    }
  }

  bool ok = putMetadata(meta_pack);

  out_msg.present(AKIpUptaneMes_PR_putMetaResp).putMetaResp()->result =
      ok ? AKInstallationResult_success : AKInstallationResult_failure;

  return ReturnCode::kOk;
}

AktualizrSecondary::ReturnCode AktualizrSecondary::installHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
  (void)in_msg;
  auto install_result = install();
  out_msg.present(AKIpUptaneMes_PR_installResp).installResp()->result =
      static_cast<AKInstallationResultCode_t>(install_result);

  if (data::ResultCode::Numeric::kNeedCompletion == install_result) {
    return ReturnCode::kRebootRequired;
  }

  return ReturnCode::kOk;
}
