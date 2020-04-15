#include "managedsecondary.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "crypto/crypto.h"
#include "logging/logging.h"
#include "uptane/manifest.h"
#include "uptane/uptanerepository.h"
#include "utilities/exceptions.h"
#include "utilities/fault_injection.h"
#include "utilities/utils.h"

namespace Primary {

ManagedSecondary::ManagedSecondary(Primary::ManagedSecondaryConfig sconfig_in, ImageReaderProvider image_reader_in)
    : sconfig(std::move(sconfig_in)), image_reader(std::move(image_reader_in)) {
  // TODO: FIX
  // loadMetadata(meta_pack);
  std::string public_key_string;

  if (!loadKeys(&public_key_string, &private_key)) {
    if (!Crypto::generateKeyPair(sconfig.key_type, &public_key_string, &private_key)) {
      LOG_ERROR << "Could not generate rsa keys for secondary " << ManagedSecondary::getSerial() << "@"
                << sconfig.ecu_hardware_id;
      throw std::runtime_error("Unable to generate secondary rsa keys");
    }

    // do not store keys yet, wait until SotaUptaneClient performed device initialization
  }
  public_key_ = PublicKey(public_key_string, sconfig.key_type);
  Initialize();
}

void ManagedSecondary::Initialize() {
  struct stat st {};

  if (!boost::filesystem::is_directory(sconfig.metadata_path)) {
    Utils::createDirectories(sconfig.metadata_path, S_IRWXU);
  }
  if (stat(sconfig.metadata_path.c_str(), &st) < 0) {
    throw std::runtime_error(std::string("Could not check metadata directory permissions: ") + std::strerror(errno));
  }
  if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
    throw std::runtime_error("Secondary metadata directory has unsafe permissions");
  }

  if (!boost::filesystem::is_directory(sconfig.full_client_dir)) {
    Utils::createDirectories(sconfig.full_client_dir, S_IRWXU);
  }
  if (stat(sconfig.full_client_dir.c_str(), &st) < 0) {
    throw std::runtime_error(std::string("Could not check client directory permissions: ") + std::strerror(errno));
  }
  if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
    throw std::runtime_error("Secondary client directory has unsafe permissions");
  }

  storeKeys(public_key_.Value(), private_key);
}

void ManagedSecondary::rawToMeta() {
  // raw meta is trusted
  current_meta.director_root =
      Uptane::Root(Uptane::RepositoryType::Director(), Utils::parseJSON(current_raw_meta.director_root));
  current_meta.director_targets = Uptane::Targets(Utils::parseJSON(current_raw_meta.director_targets));
  current_meta.image_root =
      Uptane::Root(Uptane::RepositoryType::Image(), Utils::parseJSON(current_raw_meta.image_root));
  current_meta.image_targets = Uptane::Targets(Utils::parseJSON(current_raw_meta.image_targets));
  current_meta.image_timestamp = Uptane::TimestampMeta(Utils::parseJSON(current_raw_meta.image_timestamp));
  current_meta.image_snapshot = Uptane::Snapshot(Utils::parseJSON(current_raw_meta.image_snapshot));
}

bool ManagedSecondary::putMetadata(const Uptane::RawMetaPack &meta_pack) {
  // No verification is currently performed, we can add verification in future for testing purposes
  detected_attack = "";

  current_raw_meta = meta_pack;
  rawToMeta();  // current_raw_meta -> current_meta
  if (!current_meta.isConsistent()) {
    return false;
  }
  storeMetadata(current_raw_meta);

  expected_target_name = "";
  expected_target_hashes.clear();
  expected_target_length = 0;

  bool target_found = false;

  std::vector<Uptane::Target>::const_iterator it;
  for (it = current_meta.director_targets.targets.begin(); it != current_meta.director_targets.targets.end(); ++it) {
    // TODO: what about hardware ID? Also missing in Uptane::Target
    if (it->ecus().find(getSerial()) != it->ecus().end()) {
      if (target_found) {
        detected_attack = "Duplicate entry for this ECU";
        break;
      }
      expected_target_name = it->filename();
      expected_target_hashes = it->hashes();
      expected_target_length = it->length();
      target_found = true;
    }
  }

  if (!target_found) {
    detected_attack = "No update for this ECU";
  }

  return true;
}

int ManagedSecondary::getRootVersion(const bool director) const {
  if (director) {
    return current_meta.director_root.version();
  }
  return current_meta.image_root.version();
}

bool ManagedSecondary::putRoot(const std::string &root, const bool director) {
  Uptane::Root &prev_root = (director) ? current_meta.director_root : current_meta.image_root;
  std::string &prev_raw_root = (director) ? current_raw_meta.director_root : current_raw_meta.image_root;
  Uptane::Root new_root = Uptane::Root(
      (director) ? Uptane::RepositoryType::Director() : Uptane::RepositoryType::Image(), Utils::parseJSON(root));

  // No verification is currently performed, we can add verification in future for testing purposes
  if (new_root.version() == prev_root.version() + 1) {
    prev_root = new_root;
    prev_raw_root = root;
  } else {
    detected_attack = "Tried to update Root version " + std::to_string(prev_root.version()) + " with version " +
                      std::to_string(new_root.version());
  }

  if (!current_meta.isConsistent()) {
    return false;
  }
  storeMetadata(current_raw_meta);
  return true;
}

data::ResultCode::Numeric ManagedSecondary::install(const Uptane::Target &target_name) {
  if (fiu_fail((std::string("secondary_install_") + getSerial().ToString()).c_str()) != 0) {
    // consider changing this approach of the fault injection, since the current approach impacts the non-test code flow
    // here as well as it doesn't test the installation failure on secondary from an end-to-end perspective as it
    // injects an error on the middle of the control flow that would have happened if an installation error had happened
    // in case of the virtual or the ip-secondary or any other secondary, e.g. add a mock secondary that returns an
    // error to sendFirmware/install request we might consider passing the installation description message from
    // Secondary, not just bool and/or data::ResultCode::Numeric
    return data::ResultCode::Numeric::kInstallFailed;
  }

  image_reader(target_name)->writeToFile(sconfig.firmware_path);
  Utils::writeFile(sconfig.target_name_path, expected_target_name);
  sync();
  return data::ResultCode::Numeric::kOk;
}

Uptane::Manifest ManagedSecondary::getManifest() const {
  Uptane::InstalledImageInfo firmware_info;
  if (!getFirmwareInfo(firmware_info)) {
    return Json::Value(Json::nullValue);
  }

  Json::Value manifest = Uptane::ManifestIssuer::assembleManifest(firmware_info, getSerial());
  // consider updating Uptane::ManifestIssuer functionality to fulfill the given use-case
  // and removing the following code from here so we encapsulate manifest generation
  // and signing functionality in one place
  manifest["attacks_detected"] = detected_attack;

  Json::Value signed_ecu_version;

  std::string b64sig = Utils::toBase64(Crypto::RSAPSSSign(nullptr, private_key, Utils::jsonToCanonicalStr(manifest)));
  Json::Value signature;
  signature["method"] = "rsassa-pss";
  signature["sig"] = b64sig;

  signature["keyid"] = public_key_.KeyId();
  signed_ecu_version["signed"] = manifest;
  signed_ecu_version["signatures"] = Json::Value(Json::arrayValue);
  signed_ecu_version["signatures"].append(signature);

  return signed_ecu_version;
}

void ManagedSecondary::storeKeys(const std::string &pub_key, const std::string &priv_key) {
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_private_key), priv_key);
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_public_key), pub_key);
}

bool ManagedSecondary::loadKeys(std::string *pub_key, std::string *priv_key) {
  boost::filesystem::path public_key_path = sconfig.full_client_dir / sconfig.ecu_public_key;
  boost::filesystem::path private_key_path = sconfig.full_client_dir / sconfig.ecu_private_key;

  if (!boost::filesystem::exists(public_key_path) || !boost::filesystem::exists(private_key_path)) {
    return false;
  }

  *priv_key = Utils::readFile(private_key_path.string());
  *pub_key = Utils::readFile(public_key_path.string());
  return true;
}

}  // namespace Primary
