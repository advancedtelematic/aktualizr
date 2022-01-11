#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <fstream>

#include "crypto/crypto.h"
#include "fwsecondary.h"
#include "utilities/fault_injection.h"
#include "utilities/utils.h"

#include "uptane/manifest.h"
#include "uptane/tuf.h"
#include "uptane/uptanerepository.h"

namespace Primary {

const char* const FirmwareFileSecondaryConfig::Type = "firmware";

FirmwareFileSecondaryConfig::FirmwareFileSecondaryConfig(const Json::Value& json_config) {
  partial_verifying = json_config["partial_verifying"].asBool();
  ecu_serial = json_config["ecu_serial"].asString();
  ecu_hardware_id = json_config["ecu_hardware_id"].asString();
  full_client_dir = json_config["full_client_dir"].asString();
  ecu_private_key = json_config["ecu_private_key"].asString();
  ecu_public_key = json_config["ecu_public_key"].asString();
  firmware_path = json_config["firmware_path"].asString();  //??
  target_name_path = json_config["target_name_path"].asString();
  metadata_path = json_config["metadata_path"].asString();

  // todo: more custorm options
  // path for FW metadata?
}

// TODO: need common approach for all XXXXSecondaryCongig. it is just copy/paset e.g. use templates?
std::vector<FirmwareFileSecondaryConfig> FirmwareFileSecondaryConfig::create_from_file(
    const boost::filesystem::path& file_full_path) {
  Json::Value json_config;
  std::ifstream json_file(file_full_path.string());
  Json::parseFromStream(Json::CharReaderBuilder(), json_file, &json_config, nullptr);
  json_file.close();

  std::vector<FirmwareFileSecondaryConfig> sec_configs;
  sec_configs.reserve(json_config[Type].size());

  for (const auto& item : json_config[Type]) {
    sec_configs.emplace_back(FirmwareFileSecondaryConfig(item));
  }
  return sec_configs;
}

void FirmwareFileSecondaryConfig::dump(const boost::filesystem::path& file_full_path) const {
  Json::Value json_config;

  json_config["partial_verifying"] = partial_verifying;
  json_config["ecu_serial"] = ecu_serial;
  json_config["ecu_hardware_id"] = ecu_hardware_id;
  json_config["full_client_dir"] = full_client_dir.string();
  json_config["ecu_private_key"] = ecu_private_key;
  json_config["ecu_public_key"] = ecu_public_key;
  json_config["firmware_path"] = firmware_path.string();
  json_config["target_name_path"] = target_name_path.string();
  json_config["metadata_path"] = metadata_path.string();

  Json::Value root;
  // Append to the config file if it already exists.
  if (boost::filesystem::exists(file_full_path)) {
    root = Utils::parseJSONFile(file_full_path);
  }
  root[Type].append(json_config);

  Json::StreamWriterBuilder json_bwriter;
  json_bwriter["indentation"] = "\t";
  std::unique_ptr<Json::StreamWriter> const json_writer(json_bwriter.newStreamWriter());

  boost::filesystem::create_directories(file_full_path.parent_path());
  std::ofstream json_file(file_full_path.string());
  json_writer->write(root, &json_file);
  json_file.close();
}

FirmwareFileSecondary::FirmwareFileSecondary(FirmwareFileSecondaryConfig sconfig_in)
    : Primary::ManagedSecondary(std::move(sconfig_in)) {}

data::InstallationResult FirmwareFileSecondary::putMetadata(const Uptane::Target& target) {
#ifdef FIU_ENABLE
  if (fiu_fail("fwsecondary_putmetadata") != 0) {
    return data::InstallationResult(data::ResultCode::Numeric::kVerificationFailed, fault_injection_last_info());
  }
#endif
  return ManagedSecondary::putMetadata(target);
}

data::InstallationResult FirmwareFileSecondary::putRoot(const std::string& root, bool director) {
#ifdef FIU_ENABLE
  if (fiu_fail("fwsecondary_putroot") != 0) {
    return data::InstallationResult(data::ResultCode::Numeric::kVerificationFailed, fault_injection_last_info());
  }
#endif
  return ManagedSecondary::putRoot(root, director);
}

data::InstallationResult FirmwareFileSecondary::sendFirmware(const Uptane::Target& target) {
#ifdef FIU_ENABLE
  if (fiu_fail((std::string("fwsecondary_sendfirmware_") + getSerial().ToString()).c_str()) != 0) {
    // Put the injected failure string into the ResultCode so that it shows up
    // in the device's concatenated InstallationResult.
    return data::InstallationResult(
        data::ResultCode(data::ResultCode::Numeric::kDownloadFailed, fault_injection_last_info()), "Forced failure");
  }
#endif
  return ManagedSecondary::sendFirmware(target);
}

data::InstallationResult FirmwareFileSecondary::install(const Uptane::Target& target) {
  if (fiu_fail((std::string("secondary_install_") + getSerial().ToString()).c_str()) != 0) {
    // Put the injected failure string into the ResultCode so that it shows up
    // in the device's concatenated InstallationResult.
    return data::InstallationResult(
        data::ResultCode(data::ResultCode::Numeric::kInstallFailed, fault_injection_last_info()), "Forced failure");
  }
  last_installed_len = target.length();
  last_installed_name = target.filename();
  if (!target.hashes().empty()) {
    last_installed_sha256 = target.hashes()[0].HashString();
  }

  // real implementation create link instead of install or do something else
  return data::InstallationResult(data::ResultCode::Numeric::kOk, "");
}

bool FirmwareFileSecondary::getFirmwareInfo(Uptane::InstalledImageInfo& firmware_info) const {
  LOG_DEBUG << "EcuBundleSecondary::getFirmwareInfo";

  if (last_installed_name.empty() || firmware_info.hash.empty()) {
    LOG_INFO << "FirmwareFileSecondary::getFirmwareInfo has no info for " << getSerial() << ". Use default vaules";
    std::string content = "";
    firmware_info.hash = boost::algorithm::to_lower_copy(Uptane::ManifestIssuer::generateVersionHashStr(content));
    firmware_info.len = content.size();
    firmware_info.name = std::string("noimage");
  } else {
    firmware_info.name = last_installed_name;
    firmware_info.hash = boost::algorithm::to_lower_copy(last_installed_sha256);
    firmware_info.len = last_installed_len;
  }
  return true;
}

}  // namespace Primary
