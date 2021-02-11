#include "sotauptaneclient.h"

#include <fnmatch.h>
#include <unistd.h>
#include <memory>
#include <utility>

#include "crypto/crypto.h"
#include "crypto/keymanager.h"
#include "initializer.h"
#include "libaktualizr/campaign.h"
#include "logging/logging.h"
#include "uptane/exceptions.h"

#include "utilities/fault_injection.h"
#include "utilities/utils.h"

static void report_progress_cb(event::Channel *channel, const Uptane::Target &target, const std::string &description,
                               unsigned int progress) {
  if (channel == nullptr) {
    return;
  }
  auto event = std::make_shared<event::DownloadProgressReport>(target, description, progress);
  (*channel)(event);
}

void SotaUptaneClient::addSecondary(const std::shared_ptr<SecondaryInterface> &sec) {
  Uptane::EcuSerial serial = sec->getSerial();

  const auto map_it = secondaries.find(serial);
  if (map_it != secondaries.end()) {
    throw std::runtime_error(std::string("Multiple Secondaries found with the same serial: ") + serial.ToString());
  }

  secondaries.emplace(serial, sec);
}

std::vector<Uptane::Target> SotaUptaneClient::findForEcu(const std::vector<Uptane::Target> &targets,
                                                         const Uptane::EcuSerial &ecu_id) {
  std::vector<Uptane::Target> result;
  for (auto it = targets.begin(); it != targets.end(); ++it) {
    if (it->ecus().find(ecu_id) != it->ecus().end()) {
      result.push_back(*it);
    }
  }
  return result;
}

data::InstallationResult SotaUptaneClient::PackageInstall(const Uptane::Target &target) {
  LOG_INFO << "Installing package using " << package_manager_->name() << " package manager";
  try {
    return package_manager_->install(target);
  } catch (std::exception &ex) {
    LOG_ERROR << "Installation failed: " << ex.what();
    return data::InstallationResult(data::ResultCode::Numeric::kInstallFailed, ex.what());
  }
}

void SotaUptaneClient::finalizeAfterReboot() {
  // TODO: consider bringing checkAndUpdatePendingSecondaries and the following functionality
  // to the common denominator
  if (!hasPendingUpdates()) {
    LOG_DEBUG << "No pending updates, continuing with initialization";
    return;
  }

  LOG_INFO << "Checking for a pending update to apply for Primary ECU";

  const Uptane::EcuSerial &primary_ecu_serial = primaryEcuSerial();
  boost::optional<Uptane::Target> pending_target;
  storage->loadInstalledVersions(primary_ecu_serial.ToString(), nullptr, &pending_target);

  if (!pending_target) {
    LOG_ERROR << "No pending update for Primary ECU found, continuing with initialization";
    return;
  }

  LOG_INFO << "Pending update for Primary ECU was found, trying to apply it...";

  data::InstallationResult install_res = package_manager_->finalizeInstall(*pending_target);

  if (install_res.result_code == data::ResultCode::Numeric::kNeedCompletion) {
    LOG_INFO << "Pending update for Primary ECU was not applied because reboot was not detected, "
                "continuing with initialization";
    return;
  }

  storage->saveEcuInstallationResult(primary_ecu_serial, install_res);

  const std::string correlation_id = pending_target->correlation_id();
  if (install_res.success) {
    storage->saveInstalledVersion(primary_ecu_serial.ToString(), *pending_target, InstalledVersionUpdateMode::kCurrent);

    report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(primary_ecu_serial, correlation_id, true));
  } else {
    // finalize failed, unset pending flag so that the rest of the Uptane process can go forward again
    storage->saveInstalledVersion(primary_ecu_serial.ToString(), *pending_target, InstalledVersionUpdateMode::kNone);
    report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(primary_ecu_serial, correlation_id, false));
  }

  director_repo.dropTargets(*storage);  // fix for OTA-2587, listen to backend again after end of install

  data::InstallationResult ir;
  std::string raw_report;
  computeDeviceInstallationResult(&ir, &raw_report);
  storage->storeDeviceInstallationResult(ir, raw_report, correlation_id);
  putManifestSimple();
}

data::InstallationResult SotaUptaneClient::PackageInstallSetResult(const Uptane::Target &target) {
  data::InstallationResult result;
  Uptane::EcuSerial ecu_serial = primaryEcuSerial();

  // This is to recover more gracefully if the install process was interrupted
  // but ends up booting the new version anyway (e.g: OSTree finished
  // deploying but the device restarted before the final saveInstalledVersion
  // was called).
  // By storing the version in the table (as uninstalled), we can still pick
  // up some metadata
  // TODO: we do not detect the incomplete install at aktualizr start in that
  // case, it should ideally report a meaningful error.
  storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kNone);

  result = PackageInstall(target);
  if (result.result_code.num_code == data::ResultCode::Numeric::kOk) {
    // simple case: update already completed
    storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kCurrent);
  } else if (result.result_code.num_code == data::ResultCode::Numeric::kNeedCompletion) {
    // OSTree case: need reboot
    storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kPending);
  }
  storage->saveEcuInstallationResult(ecu_serial, result);
  return result;
}

/* Hardware info is treated differently than the other device data. The default
 * info (supplied via lshw) is only sent once and never again, even if it
 * changes. (Unfortunately, it can change often due to CPU frequency scaling.)
 * However, users can provide custom info via the API, and that will be sent if
 * it has changed. */
void SotaUptaneClient::reportHwInfo(const Json::Value &custom_hwinfo) {
  Json::Value system_info;
  std::string stored_hash;
  storage->loadDeviceDataHash("hardware_info", &stored_hash);

  if (custom_hwinfo.empty()) {
    if (!stored_hash.empty()) {
      LOG_TRACE << "Not reporting default hardware information because it has already been reported";
      return;
    }
    system_info = Utils::getHardwareInfo();
    if (system_info.empty()) {
      LOG_WARNING << "Unable to fetch hardware information from host system.";
      return;
    }
  }

  const Json::Value &hw_info = custom_hwinfo.empty() ? system_info : custom_hwinfo;
  const Hash new_hash = Hash::generate(Hash::Type::kSha256, Utils::jsonToCanonicalStr(hw_info));
  if (new_hash != Hash(Hash::Type::kSha256, stored_hash)) {
    if (custom_hwinfo.empty()) {
      LOG_DEBUG << "Reporting default hardware information";
    } else {
      LOG_DEBUG << "Reporting custom hardware information";
    }
    const HttpResponse response = http->put(config.tls.server + "/system_info", hw_info);
    if (response.isOk()) {
      storage->storeDeviceDataHash("hardware_info", new_hash.HashString());
    }
  } else {
    LOG_TRACE << "Not reporting hardware information because it has not changed";
  }
}

void SotaUptaneClient::reportInstalledPackages() {
  const Json::Value packages = package_manager_->getInstalledPackages();
  const Hash new_hash = Hash::generate(Hash::Type::kSha256, Utils::jsonToCanonicalStr(packages));
  std::string stored_hash;
  if (!(storage->loadDeviceDataHash("installed_packages", &stored_hash) &&
        new_hash == Hash(Hash::Type::kSha256, stored_hash))) {
    LOG_DEBUG << "Reporting installed packages";
    const HttpResponse response = http->put(config.tls.server + "/core/installed", packages);
    if (response.isOk()) {
      storage->storeDeviceDataHash("installed_packages", new_hash.HashString());
    }
  } else {
    LOG_TRACE << "Not reporting installed packages because they have not changed";
  }
}

void SotaUptaneClient::reportNetworkInfo() {
  if (!config.telemetry.report_network) {
    LOG_TRACE << "Not reporting network information because telemetry is disabled";
    return;
  }

  Json::Value network_info;
  try {
    network_info = Utils::getNetworkInfo();
  } catch (const std::exception &ex) {
    LOG_ERROR << "Failed to get network info: " << ex.what();
    return;
  }
  const Hash new_hash = Hash::generate(Hash::Type::kSha256, Utils::jsonToCanonicalStr(network_info));
  std::string stored_hash;
  if (!(storage->loadDeviceDataHash("network_info", &stored_hash) &&
        new_hash == Hash(Hash::Type::kSha256, stored_hash))) {
    LOG_DEBUG << "Reporting network information";
    const HttpResponse response = http->put(config.tls.server + "/system_info/network", network_info);
    if (response.isOk()) {
      storage->storeDeviceDataHash("network_info", new_hash.HashString());
    }
  } else {
    LOG_TRACE << "Not reporting network information because it has not changed";
  }
}

void SotaUptaneClient::reportAktualizrConfiguration() {
  if (!config.telemetry.report_config) {
    LOG_TRACE << "Not reporting libaktualizr configuration because telemetry is disabled";
    return;
  }

  std::stringstream conf_ss;
  config.writeToStream(conf_ss);
  const std::string conf_str = conf_ss.str();
  const Hash new_hash = Hash::generate(Hash::Type::kSha256, conf_str);
  std::string stored_hash;
  if (!(storage->loadDeviceDataHash("configuration", &stored_hash) &&
        new_hash == Hash(Hash::Type::kSha256, stored_hash))) {
    LOG_DEBUG << "Reporting libaktualizr configuration";
    const HttpResponse response = http->post(config.tls.server + "/system_info/config", "application/toml", conf_str);
    if (response.isOk()) {
      storage->storeDeviceDataHash("configuration", new_hash.HashString());
    }
  } else {
    LOG_TRACE << "Not reporting libaktualizr configuration because it has not changed";
  }
}

Json::Value SotaUptaneClient::AssembleManifest() {
  Json::Value manifest;  // signed top-level
  Uptane::EcuSerial primary_ecu_serial = primaryEcuSerial();
  manifest["primary_ecu_serial"] = primary_ecu_serial.ToString();

  // first part: report current version/state of all ECUs
  Json::Value version_manifest;

  Json::Value primary_manifest = uptane_manifest->assembleManifest(package_manager_->getCurrent());
  std::vector<std::pair<Uptane::EcuSerial, int64_t>> ecu_cnt;
  std::string report_counter;
  if (!storage->loadEcuReportCounter(&ecu_cnt) || ecu_cnt.empty()) {
    LOG_ERROR << "No ECU version report counter, please check the database!";
    // TODO: consider not sending manifest at all in this case, or maybe retry
  } else {
    report_counter = std::to_string(ecu_cnt[0].second + 1);
    storage->saveEcuReportCounter(ecu_cnt[0].first, ecu_cnt[0].second + 1);
  }
  version_manifest[primary_ecu_serial.ToString()] = uptane_manifest->sign(primary_manifest, report_counter);

  for (auto it = secondaries.begin(); it != secondaries.end(); it++) {
    const Uptane::EcuSerial &ecu_serial = it->first;
    Uptane::Manifest secmanifest;
    try {
      secmanifest = it->second->getManifest();
    } catch (const std::exception &ex) {
      // Not critical; it might just be temporarily offline.
      LOG_DEBUG << "Failed to get manifest from Secondary with serial " << ecu_serial << ": " << ex.what();
    }

    bool from_cache = false;
    if (secmanifest.empty()) {
      // Could not get the Secondary manifest directly, so just use a cached value.
      std::string cached;
      if (storage->loadCachedEcuManifest(ecu_serial, &cached)) {
        LOG_WARNING << "Could not reach Secondary " << ecu_serial << ", sending a cached version of its manifest";
        secmanifest = Utils::parseJSON(cached);
        from_cache = true;
      } else {
        LOG_ERROR << "Failed to get a valid manifest from Secondary with serial " << ecu_serial << " or from cache!";
        continue;
      }
    }

    bool verified = false;
    try {
      verified = secmanifest.verifySignature(it->second->getPublicKey());
    } catch (const std::exception &ex) {
      LOG_ERROR << "Failed to get public key from Secondary with serial " << ecu_serial << ": " << ex.what();
    }
    if (verified) {
      version_manifest[ecu_serial.ToString()] = secmanifest;
      if (!from_cache) {
        storage->storeCachedEcuManifest(ecu_serial, Utils::jsonToCanonicalStr(secmanifest));
      }
    } else {
      // TODO(OTA-4305): send a corresponding event/report in this case
      LOG_ERROR << "Invalid manifest or signature reported by Secondary: "
                << " serial: " << ecu_serial << " manifest: " << secmanifest;
    }
  }
  manifest["ecu_version_manifests"] = version_manifest;

  // second part: report installation results
  Json::Value installation_report;

  data::InstallationResult dev_result;
  std::string raw_report;
  std::string correlation_id;
  bool has_results = storage->loadDeviceInstallationResult(&dev_result, &raw_report, &correlation_id);
  if (has_results) {
    if (!(dev_result.isSuccess() || dev_result.needCompletion())) {
      director_repo.dropTargets(*storage);  // fix for OTA-2587, listen to backend again after end of install
    }

    installation_report["result"] = dev_result.toJson();
    installation_report["raw_report"] = raw_report;
    installation_report["correlation_id"] = correlation_id;
    installation_report["items"] = Json::arrayValue;

    std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_results;
    storage->loadEcuInstallationResults(&ecu_results);
    for (const auto &r : ecu_results) {
      Uptane::EcuSerial serial = r.first;
      data::InstallationResult res = r.second;

      Json::Value item;
      item["ecu"] = serial.ToString();
      item["result"] = res.toJson();

      installation_report["items"].append(item);
    }

    manifest["installation_report"]["content_type"] = "application/vnd.com.here.otac.installationReport.v1";
    manifest["installation_report"]["report"] = installation_report;
  } else {
    LOG_DEBUG << "No installation result to report in manifest";
  }

  return manifest;
}

bool SotaUptaneClient::hasPendingUpdates() const { return storage->hasPendingInstall(); }

void SotaUptaneClient::initialize() {
  LOG_DEBUG << "Checking if device is provisioned...";
  auto keys = std::make_shared<KeyManager>(storage, config.keymanagerConfig());

  Initializer initializer(config.provision, storage, http, *keys, secondaries);

  EcuSerials serials;
  /* unlikely, post-condition of Initializer::Initializer() */
  if (!storage->loadEcuSerials(&serials) || serials.empty()) {
    throw std::runtime_error("Unable to load ECU serials after device registration.");
  }

  uptane_manifest = std::make_shared<Uptane::ManifestIssuer>(keys, serials[0].first);
  primary_ecu_serial_ = serials[0].first;
  primary_ecu_hw_id_ = serials[0].second;
  LOG_INFO << "Primary ECU serial: " << primary_ecu_serial_ << " with hardware ID: " << primary_ecu_hw_id_;

  for (auto it = secondaries.begin(); it != secondaries.end(); ++it) {
    try {
      it->second->init(secondary_provider_);
    } catch (const std::exception &ex) {
      LOG_ERROR << "Failed to initialize Secondary with serial " << it->first << ": " << ex.what();
    }
  }

  std::string device_id;
  if (!storage->loadDeviceId(&device_id)) {
    throw std::runtime_error("Unable to load device ID after device registration.");
  }
  LOG_INFO << "Device ID: " << device_id;
  LOG_INFO << "Device Gateway URL: " << config.tls.server;

  std::string subject;
  std::string issuer;
  std::string not_before;
  std::string not_after;
  keys->getCertInfo(&subject, &issuer, &not_before, &not_after);
  LOG_INFO << "Certificate subject: " << subject;
  LOG_INFO << "Certificate issuer: " << issuer;
  LOG_INFO << "Certificate valid from: " << not_before << " until: " << not_after;

  LOG_DEBUG << "... provisioned OK";
  finalizeAfterReboot();
}

void SotaUptaneClient::updateDirectorMeta() {
  try {
    director_repo.updateMeta(*storage, *uptane_fetcher);
  } catch (const std::exception &e) {
    LOG_ERROR << "Director metadata update failed: " << e.what();
    throw;
  }
}

void SotaUptaneClient::updateImageMeta() {
  try {
    image_repo.updateMeta(*storage, *uptane_fetcher);
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed to update Image repo metadata: " << e.what();
    throw;
  }
}

void SotaUptaneClient::checkDirectorMetaOffline() {
  try {
    director_repo.checkMetaOffline(*storage);
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed to check Director metadata: " << e.what();
    throw;
  }
}

void SotaUptaneClient::checkImageMetaOffline() {
  try {
    image_repo.checkMetaOffline(*storage);
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed to check Image repo metadata: " << e.what();
  }
}

void SotaUptaneClient::computeDeviceInstallationResult(data::InstallationResult *result,
                                                       std::string *raw_installation_report) const {
  data::InstallationResult device_installation_result =
      data::InstallationResult(data::ResultCode::Numeric::kOk, "Device has been successfully installed");
  std::string raw_ir = "Installation succesful";

  do {
    std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_results;

    if (!storage->loadEcuInstallationResults(&ecu_results)) {
      // failed to load ECUs' installation result
      device_installation_result = data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                                            "Unable to get installation results from ECUs");
      raw_ir = "Failed to load ECU installation results";
      break;
    }

    std::string result_code_err_str;

    for (const auto &r : ecu_results) {
      auto ecu_serial = r.first;
      auto installation_res = r.second;

      auto hw_id = getEcuHwId(ecu_serial);

      if (!hw_id) {
        // couldn't find any ECU with the given serial/ID
        device_installation_result = data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                                              "Unable to get installation results from ECUs");

        raw_ir = "Failed to find an ECU with the given serial: " + ecu_serial.ToString();
        break;
      }

      if (installation_res.needCompletion()) {
        // one of the ECUs needs completion, aka an installation finalization
        device_installation_result =
            data::InstallationResult(data::ResultCode::Numeric::kNeedCompletion,
                                     "ECU needs completion/finalization to be installed: " + ecu_serial.ToString());
        raw_ir = "ECU needs completion/finalization to be installed: " + ecu_serial.ToString();
        break;
      }

      // format:
      // ecu1_hwid:failure1|ecu2_hwid:failure2
      if (!installation_res.isSuccess()) {
        const std::string ecu_code_str = (*hw_id).ToString() + ":" + installation_res.result_code.toString();
        result_code_err_str += (!result_code_err_str.empty() ? "|" : "") + ecu_code_str;
      }
    }

    if (!result_code_err_str.empty()) {
      // installation on at least one of the ECUs has failed
      device_installation_result =
          data::InstallationResult(data::ResultCode(data::ResultCode::Numeric::kInstallFailed, result_code_err_str),
                                   "Installation failed on one or more ECUs");
      raw_ir = "Installation failed on one or more ECUs";

      break;
    }

  } while (false);

  if (result != nullptr) {
    *result = device_installation_result;
  }

  if (raw_installation_report != nullptr) {
    *raw_installation_report = raw_ir;
  }
}

void SotaUptaneClient::getNewTargets(std::vector<Uptane::Target> *new_targets, unsigned int *ecus_count) {
  const std::vector<Uptane::Target> targets = director_repo.getTargets().targets;
  const Uptane::EcuSerial primary_ecu_serial = primaryEcuSerial();
  if (ecus_count != nullptr) {
    *ecus_count = 0;
  }
  for (const Uptane::Target &target : targets) {
    bool is_new = false;
    for (const auto &ecu : target.ecus()) {
      const Uptane::EcuSerial ecu_serial = ecu.first;
      const Uptane::HardwareIdentifier hw_id = ecu.second;

      // 5.4.4.6.8. If checking Targets metadata from the Director repository,
      // and the ECU performing the verification is the Primary ECU, check that
      // all listed ECU identifiers correspond to ECUs that are actually present
      // in the vehicle.
      const auto hw_id_known = getEcuHwId(ecu_serial);
      if (!hw_id_known) {
        // This is triggered if a Secondary is removed after an update was
        // installed on it because of the empty targets optimization.
        // Thankfully if the Director issues new Targets, it fixes itself.
        LOG_ERROR << "Unknown ECU ID in Director Targets metadata: " << ecu_serial.ToString();
        throw Uptane::BadEcuId(target.filename());
      }

      if (*hw_id_known != hw_id) {
        LOG_ERROR << "Wrong hardware identifier for ECU " << ecu_serial.ToString();
        throw Uptane::BadHardwareId(target.filename());
      }

      boost::optional<Uptane::Target> current_version;
      if (!storage->loadInstalledVersions(ecu_serial.ToString(), &current_version, nullptr)) {
        LOG_WARNING << "Could not load currently installed version for ECU ID: " << ecu_serial.ToString();
        break;
      }

      if (!current_version) {
        LOG_WARNING << "Current version for ECU ID: " << ecu_serial.ToString() << " is unknown";
        is_new = true;
      } else if (current_version->MatchTarget(target)) {
        // Do nothing; target is already installed.
      } else if (current_version->filename() == target.filename()) {
        LOG_ERROR << "Director Target filename matches currently installed version, but content differs!";
        throw Uptane::TargetContentMismatch(target.filename());
      } else {
        is_new = true;
      }

      // Reject non-OSTree updates for the Primary if using OSTree.
      // TODO(OTA-4939): Unify this with the check in
      // PackageManagerFake::fetchTarget() and make it more generic.
      if (primary_ecu_serial == ecu_serial) {
        if (!target.IsOstree() && config.pacman.type == PACKAGE_MANAGER_OSTREE) {
          LOG_ERROR << "Cannot install a non-OSTree package on an OSTree system";
          throw Uptane::InvalidTarget(target.filename());
        }
      }

      if (is_new && ecus_count != nullptr) {
        (*ecus_count)++;
      }
      // no updates for this image => continue
    }
    if (is_new) {
      new_targets->push_back(target);
    }
  }
}

std::unique_ptr<Uptane::Target> SotaUptaneClient::findTargetHelper(const Uptane::Targets &cur_targets,
                                                                   const Uptane::Target &queried_target,
                                                                   const int level, const bool terminating,
                                                                   const bool offline) {
  TargetCompare target_comp(queried_target);
  const auto it = std::find_if(cur_targets.targets.cbegin(), cur_targets.targets.cend(), target_comp);
  if (it != cur_targets.targets.cend()) {
    return std_::make_unique<Uptane::Target>(*it);
  }

  if (terminating || level >= Uptane::kDelegationsMaxDepth) {
    return std::unique_ptr<Uptane::Target>(nullptr);
  }

  for (const auto &delegate_name : cur_targets.delegated_role_names_) {
    Uptane::Role delegate_role = Uptane::Role::Delegation(delegate_name);
    auto patterns = cur_targets.paths_for_role_.find(delegate_role);
    if (patterns == cur_targets.paths_for_role_.end()) {
      continue;
    }

    bool match = false;
    for (const auto &pattern : patterns->second) {
      if (fnmatch(pattern.c_str(), queried_target.filename().c_str(), 0) == 0) {
        match = true;
        break;
      }
    }
    if (!match) {
      continue;
    }

    // Target name matches one of the patterns

    auto delegation =
        Uptane::getTrustedDelegation(delegate_role, cur_targets, image_repo, *storage, *uptane_fetcher, offline);
    if (delegation.isExpired(TimeStamp::Now())) {
      continue;
    }

    auto is_terminating = cur_targets.terminating_role_.find(delegate_role);
    if (is_terminating == cur_targets.terminating_role_.end()) {
      throw Uptane::Exception("image", "Inconsistent delegations");
    }

    auto found_target = findTargetHelper(delegation, queried_target, level + 1, is_terminating->second, offline);
    if (found_target != nullptr) {
      return found_target;
    }
  }

  return std::unique_ptr<Uptane::Target>(nullptr);
}

std::unique_ptr<Uptane::Target> SotaUptaneClient::findTargetInDelegationTree(const Uptane::Target &target,
                                                                             const bool offline) {
  auto toplevel_targets = image_repo.getTargets();
  if (toplevel_targets == nullptr) {
    return std::unique_ptr<Uptane::Target>(nullptr);
  }

  return findTargetHelper(*toplevel_targets, target, 0, false, offline);
}

result::Download SotaUptaneClient::downloadImages(const std::vector<Uptane::Target> &targets,
                                                  const api::FlowControlToken *token) {
  // Uptane step 4 - download all the images and verify them against the metadata (for OSTree - pull without
  // deploying)
  std::lock_guard<std::mutex> guard(download_mutex);
  result::Download result;
  std::vector<Uptane::Target> downloaded_targets;

  result::UpdateStatus update_status;
  try {
    update_status = checkUpdatesOffline(targets);
  } catch (const std::exception &e) {
    last_exception = std::current_exception();
    update_status = result::UpdateStatus::kError;
  }

  if (update_status == result::UpdateStatus::kNoUpdatesAvailable) {
    result = result::Download({}, result::DownloadStatus::kNothingToDownload, "");
  } else if (update_status == result::UpdateStatus::kError) {
    result = result::Download(downloaded_targets, result::DownloadStatus::kError, "Error rechecking stored metadata.");
    storeInstallationFailure(
        data::InstallationResult(data::ResultCode::Numeric::kInternalError, "Error rechecking stored metadata."));
  }

  if (update_status != result::UpdateStatus::kUpdatesAvailable) {
    sendEvent<event::AllDownloadsComplete>(result);
    return result;
  }

  for (const auto &target : targets) {
    auto res = downloadImage(target, token);
    if (res.first) {
      downloaded_targets.push_back(res.second);
    }
  }

  if (targets.size() == downloaded_targets.size()) {
    result = result::Download(downloaded_targets, result::DownloadStatus::kSuccess, "");
  } else {
    if (downloaded_targets.empty()) {
      LOG_ERROR << "0 of " << targets.size() << " targets were successfully downloaded.";
      result = result::Download(downloaded_targets, result::DownloadStatus::kError, "Each target download has failed");
    } else {
      LOG_ERROR << "Only " << downloaded_targets.size() << " of " << targets.size() << " were successfully downloaded.";
      result = result::Download(downloaded_targets, result::DownloadStatus::kPartialSuccess, "");
    }
    storeInstallationFailure(
        data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed, "Target download failed."));
  }

  sendEvent<event::AllDownloadsComplete>(result);
  return result;
}

void SotaUptaneClient::reportPause() {
  const std::string &correlation_id = director_repo.getCorrelationId();
  report_queue->enqueue(std_::make_unique<DevicePausedReport>(correlation_id));
}

void SotaUptaneClient::reportResume() {
  const std::string &correlation_id = director_repo.getCorrelationId();
  report_queue->enqueue(std_::make_unique<DeviceResumedReport>(correlation_id));
}

std::pair<bool, Uptane::Target> SotaUptaneClient::downloadImage(const Uptane::Target &target,
                                                                const api::FlowControlToken *token) {
  const std::string &correlation_id = director_repo.getCorrelationId();
  // send an event for all ECUs that are touched by this target
  for (const auto &ecu : target.ecus()) {
    report_queue->enqueue(std_::make_unique<EcuDownloadStartedReport>(ecu.first, correlation_id));
  }

  // Note: handle exceptions from here so that we can send reports and
  // DownloadTargetComple events in all cases. We might want to move these to
  // downloadImages but aktualizr-lite currently calls this method directly.

  bool success = false;
  try {
    KeyManager keys(storage, config.keymanagerConfig());
    keys.loadKeys();
    auto prog_cb = [this](const Uptane::Target &t, const std::string &description, unsigned int progress) {
      report_progress_cb(events_channel.get(), t, description, progress);
    };

    const Uptane::EcuSerial &primary_ecu_serial = primaryEcuSerial();

    if (target.IsForEcu(primary_ecu_serial) || !target.IsOstree()) {
      const int max_tries = 3;
      int tries = 0;
      std::chrono::milliseconds wait(500);

      for (; tries < max_tries; tries++) {
        success = package_manager_->fetchTarget(target, *uptane_fetcher, keys, prog_cb, token);
        // Skip trying to fetch the 'target' if control flow token transaction
        // was set to the 'abort' or 'pause' state, see the CommandQueue and FlowControlToken.
        if (success || (token != nullptr && !token->canContinue(false))) {
          break;
        } else if (tries < max_tries - 1) {
          std::this_thread::sleep_for(wait);
          wait *= 2;
        }
      }
      if (!success) {
        LOG_ERROR << "Download unsuccessful after " << tries << " attempts.";
        // TODO: Throw more meaningful exceptions. Failure can be caused by more
        // than just a hash mismatch. However, this is purely internal and
        // mostly just relevant for testing.
        throw Uptane::TargetHashMismatch(target.filename());
      }
    } else {
      // we emulate successfull download in case of the Secondary OSTree update
      success = true;
    }
  } catch (const std::exception &e) {
    LOG_ERROR << "Error downloading image: " << e.what();
    last_exception = std::current_exception();
  }

  // send this asynchronously before `sendEvent`, so that the report timestamp
  // would not be delayed by callbacks on events
  for (const auto &ecu : target.ecus()) {
    report_queue->enqueue(std_::make_unique<EcuDownloadCompletedReport>(ecu.first, correlation_id, success));
  }

  sendEvent<event::DownloadTargetComplete>(target, success);
  return {success, target};
}

void SotaUptaneClient::uptaneIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count) {
  updateDirectorMeta();

  std::vector<Uptane::Target> tmp_targets;
  unsigned int ecus;
  try {
    getNewTargets(&tmp_targets, &ecus);
  } catch (const std::exception &e) {
    LOG_ERROR << "Inconsistency between Director metadata and available ECUs: " << e.what();
    throw;
  }

  if (!tmp_targets.empty()) {
    LOG_INFO << "New updates found in Director metadata. Checking Image repo metadata...";
    updateImageMeta();
  }

  if (targets != nullptr) {
    *targets = std::move(tmp_targets);
  }
  if (ecus_count != nullptr) {
    *ecus_count = ecus;
  }
}

void SotaUptaneClient::uptaneOfflineIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count) {
  checkDirectorMetaOffline();

  std::vector<Uptane::Target> tmp_targets;
  unsigned int ecus;
  try {
    getNewTargets(&tmp_targets, &ecus);
  } catch (const std::exception &e) {
    LOG_ERROR << "Inconsistency between Director metadata and available ECUs: " << e.what();
    throw;
  }

  if (!tmp_targets.empty()) {
    LOG_DEBUG << "New updates found in stored Director metadata. Checking stored Image repo metadata...";
    checkImageMetaOffline();
  }

  if (targets != nullptr) {
    *targets = std::move(tmp_targets);
  }
  if (ecus_count != nullptr) {
    *ecus_count = ecus;
  }
}

void SotaUptaneClient::sendDeviceData(const Json::Value &custom_hwinfo) {
  reportHwInfo(custom_hwinfo);
  reportInstalledPackages();
  reportNetworkInfo();
  reportAktualizrConfiguration();
  sendEvent<event::SendDeviceDataComplete>();
}

result::UpdateCheck SotaUptaneClient::fetchMeta() {
  result::UpdateCheck result;

  reportNetworkInfo();

  if (hasPendingUpdates()) {
    // if there are some pending updates check if the Secondaries' pending updates have been applied
    LOG_INFO << "The current update is pending. Check if pending ECUs has been already updated";
    checkAndUpdatePendingSecondaries();
  }

  if (hasPendingUpdates()) {
    // if there are still some pending updates just return, don't check for new updates
    // no need in update checking if there are some pending updates
    LOG_INFO << "An update is pending. Skipping check for update until installation is complete.";
    return result::UpdateCheck({}, 0, result::UpdateStatus::kError, Json::nullValue,
                               "There are pending updates, no new updates are checked");
  }

  // Uptane step 1 (build the vehicle version manifest):
  if (!putManifestSimple()) {
    LOG_ERROR << "Error sending manifest!";
  }
  result = checkUpdates();
  sendEvent<event::UpdateCheckComplete>(result);

  return result;
}

result::UpdateCheck SotaUptaneClient::checkUpdates() {
  result::UpdateCheck result;

  std::vector<Uptane::Target> updates;
  unsigned int ecus_count = 0;
  try {
    uptaneIteration(&updates, &ecus_count);
  } catch (const std::exception &e) {
    last_exception = std::current_exception();
    result = result::UpdateCheck({}, 0, result::UpdateStatus::kError, Json::nullValue, "Could not update metadata.");
    return result;
  }

  std::string director_targets;
  if (!storage->loadNonRoot(&director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets())) {
    result = result::UpdateCheck({}, 0, result::UpdateStatus::kError, Json::nullValue, "Could not update metadata.");
    return result;
  }

  if (updates.empty()) {
    LOG_DEBUG << "No new updates found in Uptane metadata.";
    result =
        result::UpdateCheck({}, 0, result::UpdateStatus::kNoUpdatesAvailable, Utils::parseJSON(director_targets), "");
    return result;
  }

  // 5.4.4.2.10.: Verify that Targets metadata from the Director and Image
  // repositories match. A Primary ECU MUST perform this check on metadata for
  // all images listed in the Targets metadata file from the Director
  // repository.
  try {
    for (auto &target : updates) {
      auto image_target = findTargetInDelegationTree(target, false);
      if (image_target == nullptr) {
        // TODO: Could also be a missing target or delegation expiration.
        LOG_ERROR << "No matching target in Image repo Targets metadata for " << target;
        throw Uptane::TargetMismatch(target.filename());
      }
      // If the URL from the Director is unset, but the URL from the Image repo
      // is set, use that.
      if (target.uri().empty() && !image_target->uri().empty()) {
        target.setUri(image_target->uri());
      }
    }
  } catch (const std::exception &e) {
    last_exception = std::current_exception();
    LOG_ERROR << e.what();
    result = result::UpdateCheck({}, 0, result::UpdateStatus::kError, Utils::parseJSON(director_targets),
                                 "Target mismatch.");
    storeInstallationFailure(
        data::InstallationResult(data::ResultCode::Numeric::kVerificationFailed, "Metadata verification failed."));
    return result;
  }

  result = result::UpdateCheck(updates, ecus_count, result::UpdateStatus::kUpdatesAvailable,
                               Utils::parseJSON(director_targets), "");
  if (updates.size() == 1) {
    LOG_INFO << "1 new update found in both Director and Image repo metadata.";
  } else {
    LOG_INFO << updates.size() << " new updates found in both Director and Image repo metadata.";
  }
  return result;
}

result::UpdateStatus SotaUptaneClient::checkUpdatesOffline(const std::vector<Uptane::Target> &targets) {
  if (hasPendingUpdates()) {
    // no need in update checking if there are some pending updates
    LOG_INFO << "An update is pending. Skipping stored metadata check until installation is complete.";
    return result::UpdateStatus::kError;
  }

  if (targets.empty()) {
    LOG_WARNING << "Requested targets vector is empty. Nothing to do.";
    return result::UpdateStatus::kError;
  }

  std::vector<Uptane::Target> director_targets;
  unsigned int ecus_count = 0;
  try {
    uptaneOfflineIteration(&director_targets, &ecus_count);
  } catch (const std::exception &e) {
    LOG_ERROR << "Aborting; invalid Uptane metadata in storage.";
    throw;
  }

  if (director_targets.empty()) {
    LOG_ERROR << "No new updates found while rechecking stored Director Targets metadata, but " << targets.size()
              << " target(s) were requested.";
    return result::UpdateStatus::kNoUpdatesAvailable;
  }

  // For every target in the Director Targets metadata, walk the delegation
  // tree (if necessary) and find a matching target in the Image repo
  // metadata.
  for (const auto &target : targets) {
    TargetCompare target_comp(target);
    const auto it = std::find_if(director_targets.cbegin(), director_targets.cend(), target_comp);
    if (it == director_targets.cend()) {
      LOG_ERROR << "No matching target in Director Targets metadata for " << target;
      throw Uptane::Exception(Uptane::RepositoryType::DIRECTOR, "No matching target in Director Targets metadata");
    }

    const auto image_target = findTargetInDelegationTree(target, true);
    if (image_target == nullptr) {
      LOG_ERROR << "No matching target in Image repo Targets metadata for " << target;
      throw Uptane::Exception(Uptane::RepositoryType::IMAGE, "No matching target in Director Targets metadata");
    }
  }

  return result::UpdateStatus::kUpdatesAvailable;
}

result::Install SotaUptaneClient::uptaneInstall(const std::vector<Uptane::Target> &updates) {
  const std::string &correlation_id = director_repo.getCorrelationId();

  // put most of the logic in a lambda so that we can take care of common
  // post-operations
  result::Install r;
  std::string raw_report;

  std::tie(r, raw_report) = [this, &updates, &correlation_id]() -> std::tuple<result::Install, std::string> {
    result::Install result;

    // Recheck the Uptane metadata and make sure the requested updates are
    // consistent with the stored metadata.
    result::UpdateStatus update_status;
    try {
      update_status = checkUpdatesOffline(updates);
    } catch (const std::exception &e) {
      last_exception = std::current_exception();
      update_status = result::UpdateStatus::kError;
    }

    if (update_status != result::UpdateStatus::kUpdatesAvailable) {
      if (update_status == result::UpdateStatus::kNoUpdatesAvailable) {
        result.dev_report = {false, data::ResultCode::Numeric::kAlreadyProcessed, ""};
      } else {
        result.dev_report = {false, data::ResultCode::Numeric::kInternalError, ""};
      }
      return std::make_tuple(result, "Stored Uptane metadata is invalid");
    }

    Uptane::EcuSerial primary_ecu_serial = primaryEcuSerial();
    // Recheck the downloaded update hashes.
    for (const auto &update : updates) {
      if (update.IsForEcu(primary_ecu_serial) || !update.IsOstree()) {
        // download binary images for any target, for both Primary and Secondary
        // download an OSTree revision just for Primary, Secondary will do it by itself
        // Primary cannot verify downloaded OSTree targets for Secondaries,
        // Downloading of Secondary's OSTree repo revision to the Primary's can fail
        // if they differ signficantly as OSTree has a certain cap/limit of the diff it pulls
        if (package_manager_->verifyTarget(update) != TargetStatus::kGood) {
          result.dev_report = {false, data::ResultCode::Numeric::kInternalError, ""};
          return std::make_tuple(result, "Downloaded target is invalid");
        }
      }
    }

    // wait some time for Secondaries to come up
    // note: this fail after a time out but will be retried at the next install
    // phase if the targets have not been changed. This is done to avoid being
    // stuck in an unrecoverable state here
    if (!waitSecondariesReachable(updates)) {
      result.dev_report = {false, data::ResultCode::Numeric::kInternalError, "Unreachable Secondary"};
      return std::make_tuple(result, "Secondaries were not available");
    }

    // Uptane step 5 (send time to all ECUs) is not implemented yet.
    std::vector<Uptane::Target> primary_updates = findForEcu(updates, primary_ecu_serial);

    //   6 - send metadata to all the ECUs
    data::InstallationResult metadata_res;
    std::string rr;
    sendMetadataToEcus(updates, &metadata_res, &rr);
    if (!metadata_res.isSuccess()) {
      result.dev_report = std::move(metadata_res);
      return std::make_tuple(result, rr);
    }

    //   7 - send images to ECUs (deploy for OSTree)
    if (!primary_updates.empty()) {
      // assuming one OSTree OS per Primary => there can be only one OSTree update
      Uptane::Target primary_update = primary_updates[0];
      primary_update.setCorrelationId(correlation_id);

      report_queue->enqueue(std_::make_unique<EcuInstallationStartedReport>(primary_ecu_serial, correlation_id));
      sendEvent<event::InstallStarted>(primary_ecu_serial);

      data::InstallationResult install_res;
      // notify the bootloader before installation happens, because installation is not atomic and
      //   a false notification doesn't hurt when rollbacks are implemented
      package_manager_->updateNotify();
      install_res = PackageInstallSetResult(primary_update);
      if (install_res.result_code.num_code == data::ResultCode::Numeric::kNeedCompletion) {
        // update needs a reboot, send distinct EcuInstallationApplied event
        report_queue->enqueue(std_::make_unique<EcuInstallationAppliedReport>(primary_ecu_serial, correlation_id));
        sendEvent<event::InstallTargetComplete>(primary_ecu_serial, true);
      } else if (install_res.result_code.num_code == data::ResultCode::Numeric::kOk) {
        storage->saveEcuInstallationResult(primary_ecu_serial, install_res);
        report_queue->enqueue(
            std_::make_unique<EcuInstallationCompletedReport>(primary_ecu_serial, correlation_id, true));
        sendEvent<event::InstallTargetComplete>(primary_ecu_serial, true);
      } else {
        // general error case
        storage->saveEcuInstallationResult(primary_ecu_serial, install_res);
        report_queue->enqueue(
            std_::make_unique<EcuInstallationCompletedReport>(primary_ecu_serial, correlation_id, false));
        sendEvent<event::InstallTargetComplete>(primary_ecu_serial, false);
      }
      result.ecu_reports.emplace(result.ecu_reports.begin(), primary_update, primary_ecu_serial, install_res);
    } else {
      LOG_INFO << "No update to install on Primary";
    }

    auto sec_reports = sendImagesToEcus(updates);
    result.ecu_reports.insert(result.ecu_reports.end(), sec_reports.begin(), sec_reports.end());
    computeDeviceInstallationResult(&result.dev_report, &rr);

    return std::make_tuple(result, rr);
  }();

  storage->storeDeviceInstallationResult(r.dev_report, raw_report, correlation_id);

  sendEvent<event::AllInstallsComplete>(r);

  return r;
}

result::CampaignCheck SotaUptaneClient::campaignCheck() {
  auto campaigns = campaign::Campaign::fetchAvailableCampaigns(*http, config.tls.server);
  for (const auto &c : campaigns) {
    LOG_INFO << "Campaign: " << c.name;
    LOG_INFO << "Campaign id: " << c.id;
    LOG_INFO << "Campaign size: " << c.size;
    LOG_INFO << "CampaignAccept required: " << (c.autoAccept ? "no" : "yes");
    LOG_INFO << "Message: " << c.description;
  }
  result::CampaignCheck result(campaigns);
  sendEvent<event::CampaignCheckComplete>(result);
  return result;
}

void SotaUptaneClient::campaignAccept(const std::string &campaign_id) {
  sendEvent<event::CampaignAcceptComplete>();
  report_queue->enqueue(std_::make_unique<CampaignAcceptedReport>(campaign_id));
}

void SotaUptaneClient::campaignDecline(const std::string &campaign_id) {
  sendEvent<event::CampaignDeclineComplete>();
  report_queue->enqueue(std_::make_unique<CampaignDeclinedReport>(campaign_id));
}

void SotaUptaneClient::campaignPostpone(const std::string &campaign_id) {
  sendEvent<event::CampaignPostponeComplete>();
  report_queue->enqueue(std_::make_unique<CampaignPostponedReport>(campaign_id));
}

bool SotaUptaneClient::isInstallCompletionRequired() const {
  std::vector<std::pair<Uptane::EcuSerial, Hash>> pending_ecus;
  storage->getPendingEcus(&pending_ecus);
  bool pending_for_ecu = std::find_if(pending_ecus.begin(), pending_ecus.end(),
                                      [this](const std::pair<Uptane::EcuSerial, Hash> &ecu) -> bool {
                                        return ecu.first == primary_ecu_serial_;
                                      }) != pending_ecus.end();

  return pending_for_ecu && config.uptane.force_install_completion;
}

void SotaUptaneClient::completeInstall() const {
  if (isInstallCompletionRequired()) {
    package_manager_->completeInstall();
  }
}

bool SotaUptaneClient::putManifestSimple(const Json::Value &custom) {
  // does not send event, so it can be used as a subset of other steps
  if (hasPendingUpdates()) {
    // Debug level here because info level is annoying if the update check
    // frequency is low.
    LOG_DEBUG << "An update is pending. Skipping manifest upload until installation is complete.";
    return false;
  }

  static bool connected = true;
  auto manifest = AssembleManifest();
  if (!custom.empty()) {
    manifest["custom"] = custom;
  }
  auto signed_manifest = uptane_manifest->sign(manifest);
  HttpResponse response = http->put(config.uptane.director_server + "/manifest", signed_manifest);
  if (response.isOk()) {
    if (!connected) {
      LOG_INFO << "Connectivity is restored.";
    }
    connected = true;
    storage->clearInstallationResults();

    return true;
  } else {
    connected = false;
  }

  LOG_WARNING << "Put manifest request failed: " << response.getStatusStr();
  return false;
}

bool SotaUptaneClient::putManifest(const Json::Value &custom) {
  bool success = putManifestSimple(custom);
  sendEvent<event::PutManifestComplete>(success);
  return success;
}

bool SotaUptaneClient::waitSecondariesReachable(const std::vector<Uptane::Target> &updates) {
  std::map<Uptane::EcuSerial, SecondaryInterface *> targeted_secondaries;
  const Uptane::EcuSerial &primary_ecu_serial = primaryEcuSerial();
  for (const auto &t : updates) {
    for (const auto &ecu : t.ecus()) {
      if (ecu.first == primary_ecu_serial) {
        continue;
      }
      auto f = secondaries.find(ecu.first);
      if (f == secondaries.end()) {
        LOG_ERROR << "Target " << t << " has an unknown ECU serial.";
        continue;
      }

      targeted_secondaries[ecu.first] = f->second.get();
    }
  }

  if (targeted_secondaries.empty()) {
    return true;
  }

  LOG_INFO << "Waiting for Secondaries to connect to start installation...";

  auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(config.uptane.secondary_preinstall_wait_sec);
  while (std::chrono::system_clock::now() <= deadline) {
    if (targeted_secondaries.empty()) {
      return true;
    }

    for (auto sec_it = targeted_secondaries.begin(); sec_it != targeted_secondaries.end();) {
      bool connected = false;
      try {
        connected = sec_it->second->ping();
      } catch (const std::exception &ex) {
        LOG_DEBUG << "Failed to ping Secondary with serial " << sec_it->first << ": " << ex.what();
      }
      if (connected) {
        sec_it = targeted_secondaries.erase(sec_it);
      } else {
        sec_it++;
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  for (const auto &sec : targeted_secondaries) {
    LOG_ERROR << "Secondary with serial " << sec.second->getSerial() << " failed to connect!";
  }

  return false;
}

void SotaUptaneClient::storeInstallationFailure(const data::InstallationResult &result) {
  // Store installation report to inform Director of the update failure before
  // we actually got to the install step.
  const std::string &correlation_id = director_repo.getCorrelationId();
  storage->storeDeviceInstallationResult(result, "", correlation_id);
  // Fix for OTA-2587, listen to backend again after end of install.
  director_repo.dropTargets(*storage);
}

/* If the Root has been rotated more than once, we need to provide the Secondary
 * with the incremental steps from what it has now. */
data::InstallationResult SotaUptaneClient::rotateSecondaryRoot(Uptane::RepositoryType repo,
                                                               SecondaryInterface &secondary) {
  std::string latest_root;
  if (!storage->loadLatestRoot(&latest_root, repo)) {
    LOG_ERROR << "Error reading Root metadata";
    return data::InstallationResult(data::ResultCode::Numeric::kInternalError, "Error reading Root metadata");
  }

  data::InstallationResult result{data::ResultCode::Numeric::kOk, ""};
  const int last_root_version = Uptane::extractVersionUntrusted(latest_root);
  const int sec_root_version = secondary.getRootVersion((repo == Uptane::RepositoryType::Director()));
  if (sec_root_version > 0 && last_root_version - sec_root_version > 1) {
    for (int v = sec_root_version + 1; v <= last_root_version; v++) {
      std::string root;
      if (!storage->loadRoot(&root, repo, Uptane::Version(v))) {
        LOG_WARNING << "Couldn't find Root metadata in the storage, trying remote repo";
        try {
          uptane_fetcher->fetchRole(&root, Uptane::kMaxRootSize, repo, Uptane::Role::Root(), Uptane::Version(v));
        } catch (const std::exception &e) {
          // TODO(OTA-4552): looks problematic, robust procedure needs to be defined
          LOG_ERROR << "Root metadata could not be fetched, skipping to the next Secondary";
          result = data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                            "Root metadata could not be fetched, skipping to the next Secondary");
          break;
        }
      }
      try {
        result = secondary.putRoot(root, repo == Uptane::RepositoryType::Director());
      } catch (const std::exception &ex) {
        result = data::InstallationResult(data::ResultCode::Numeric::kInternalError, ex.what());
      }
      if (!result.isSuccess()) {
        LOG_ERROR << "Sending metadata to " << secondary.getSerial() << " failed: " << result.result_code << " "
                  << result.description;
        break;
      }
    }
  }
  return result;
}

// TODO: the function blocks until it updates all the Secondaries. Consider non-blocking operation.
void SotaUptaneClient::sendMetadataToEcus(const std::vector<Uptane::Target> &targets, data::InstallationResult *result,
                                          std::string *raw_installation_report) {
  data::InstallationResult final_result{data::ResultCode::Numeric::kOk, ""};
  std::string result_code_err_str;
  for (const auto &target : targets) {
    for (const auto &ecu : target.ecus()) {
      const Uptane::EcuSerial ecu_serial = ecu.first;
      const Uptane::HardwareIdentifier hw_id = ecu.second;
      auto sec = secondaries.find(ecu_serial);
      if (sec == secondaries.end()) {
        continue;
      }

      data::InstallationResult local_result{data::ResultCode::Numeric::kOk, ""};
      do {
        /* Root rotation if necessary */
        local_result = rotateSecondaryRoot(Uptane::RepositoryType::Director(), *(sec->second));
        if (!local_result.isSuccess()) {
          final_result = local_result;
          break;
        }
        local_result = rotateSecondaryRoot(Uptane::RepositoryType::Image(), *(sec->second));
        if (!local_result.isSuccess()) {
          final_result = local_result;
          break;
        }
        try {
          local_result = sec->second->putMetadata(target);
        } catch (const std::exception &ex) {
          local_result = data::InstallationResult(data::ResultCode::Numeric::kInternalError, ex.what());
        }
      } while (false);
      if (!local_result.isSuccess()) {
        LOG_ERROR << "Sending metadata to " << sec->first << " failed: " << local_result.result_code << " "
                  << local_result.description;
        const std::string ecu_code_str = hw_id.ToString() + ":" + local_result.result_code.toString();
        result_code_err_str += (!result_code_err_str.empty() ? "|" : "") + ecu_code_str;
      }
    }
  }

  if (!result_code_err_str.empty()) {
    // Sending the metadata to at least one of the ECUs has failed.
    final_result =
        data::InstallationResult(data::ResultCode(data::ResultCode::Numeric::kVerificationFailed, result_code_err_str),
                                 "Sending metadata to one or more ECUs failed");
    if (raw_installation_report != nullptr) {
      *raw_installation_report = "Sending metadata to one or more ECUs failed";
    }
  }

  if (result != nullptr) {
    *result = final_result;
  }
}

std::future<data::InstallationResult> SotaUptaneClient::sendFirmwareAsync(SecondaryInterface &secondary,
                                                                          const Uptane::Target &target) {
  auto f = [this, &secondary, target]() {
    const std::string &correlation_id = director_repo.getCorrelationId();

    sendEvent<event::InstallStarted>(secondary.getSerial());
    report_queue->enqueue(std_::make_unique<EcuInstallationStartedReport>(secondary.getSerial(), correlation_id));

    data::InstallationResult result;
    try {
      result = secondary.sendFirmware(target);
      if (result.isSuccess()) {
        result = secondary.install(target);
      }
    } catch (const std::exception &ex) {
      result = data::InstallationResult(data::ResultCode::Numeric::kInternalError, ex.what());
    }

    if (result.result_code == data::ResultCode::Numeric::kNeedCompletion) {
      report_queue->enqueue(std_::make_unique<EcuInstallationAppliedReport>(secondary.getSerial(), correlation_id));
    } else {
      report_queue->enqueue(
          std_::make_unique<EcuInstallationCompletedReport>(secondary.getSerial(), correlation_id, result.isSuccess()));
    }

    sendEvent<event::InstallTargetComplete>(secondary.getSerial(), result.isSuccess());
    return result;
  };

  return std::async(std::launch::async, f);
}

std::vector<result::Install::EcuReport> SotaUptaneClient::sendImagesToEcus(const std::vector<Uptane::Target> &targets) {
  std::vector<result::Install::EcuReport> reports;
  std::vector<std::pair<result::Install::EcuReport, std::future<data::InstallationResult>>> firmwareFutures;

  const Uptane::EcuSerial &primary_ecu_serial = primaryEcuSerial();
  // target images should already have been downloaded to metadata_path/targets/
  for (auto targets_it = targets.cbegin(); targets_it != targets.cend(); ++targets_it) {
    for (auto ecus_it = targets_it->ecus().cbegin(); ecus_it != targets_it->ecus().cend(); ++ecus_it) {
      const Uptane::EcuSerial &ecu_serial = ecus_it->first;

      if (primary_ecu_serial == ecu_serial) {
        continue;
      }

      auto f = secondaries.find(ecu_serial);
      if (f == secondaries.end()) {
        LOG_ERROR << "Target " << *targets_it << " has an unknown ECU serial";
        continue;
      }

      SecondaryInterface &sec = *f->second;
      firmwareFutures.emplace_back(result::Install::EcuReport(*targets_it, ecu_serial, data::InstallationResult()),
                                   sendFirmwareAsync(sec, *targets_it));
    }
  }

  for (auto &f : firmwareFutures) {
    data::InstallationResult fut_result = f.second.get();

    if (fut_result.isSuccess() || fut_result.result_code == data::ResultCode::Numeric::kNeedCompletion) {
      f.first.update.setCorrelationId(director_repo.getCorrelationId());
      auto update_mode =
          fut_result.isSuccess() ? InstalledVersionUpdateMode::kCurrent : InstalledVersionUpdateMode::kPending;
      storage->saveInstalledVersion(f.first.serial.ToString(), f.first.update, update_mode);
    }

    f.first.install_res = fut_result;
    storage->saveEcuInstallationResult(f.first.serial, f.first.install_res);
    reports.push_back(f.first);
  }
  return reports;
}

Uptane::LazyTargetsList SotaUptaneClient::allTargets() const {
  return Uptane::LazyTargetsList(image_repo, storage, uptane_fetcher);
}

void SotaUptaneClient::checkAndUpdatePendingSecondaries() {
  std::vector<std::pair<Uptane::EcuSerial, Hash>> pending_ecus;
  storage->getPendingEcus(&pending_ecus);

  for (const auto &pending_ecu : pending_ecus) {
    if (primaryEcuSerial() == pending_ecu.first) {
      continue;
    }
    auto &sec = secondaries[pending_ecu.first];
    Uptane::Manifest manifest;
    try {
      manifest = sec->getManifest();
    } catch (const std::exception &ex) {
      LOG_DEBUG << "Failed to get manifest from Secondary with serial " << pending_ecu.first << ": " << ex.what();
      continue;
    }
    if (manifest.empty()) {
      LOG_DEBUG << "Failed to get manifest from Secondary with serial " << pending_ecu.first;
      continue;
    }
    bool verified = false;
    try {
      verified = manifest.verifySignature(sec->getPublicKey());
    } catch (const std::exception &ex) {
      LOG_ERROR << "Failed to get public key from Secondary with serial " << pending_ecu.first << ": " << ex.what();
    }
    if (!verified) {
      LOG_ERROR << "Invalid manifest or signature reported by Secondary: "
                << " serial: " << pending_ecu.first << " manifest: " << manifest;
      continue;
    }
    auto current_ecu_hash = manifest.installedImageHash();
    if (pending_ecu.second == current_ecu_hash) {
      LOG_INFO << "The pending update " << current_ecu_hash << " has been installed on " << pending_ecu.first;
      boost::optional<Uptane::Target> pending_version;
      if (storage->loadInstalledVersions(pending_ecu.first.ToString(), nullptr, &pending_version)) {
        storage->saveEcuInstallationResult(pending_ecu.first,
                                           data::InstallationResult(data::ResultCode::Numeric::kOk, ""));

        storage->saveInstalledVersion(pending_ecu.first.ToString(), *pending_version,
                                      InstalledVersionUpdateMode::kCurrent);

        report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(
            pending_ecu.first, pending_version->correlation_id(), true));

        data::InstallationResult ir;
        std::string raw_report;
        computeDeviceInstallationResult(&ir, &raw_report);
        storage->storeDeviceInstallationResult(ir, raw_report, pending_version->correlation_id());
      }
    }
  }
}

boost::optional<Uptane::HardwareIdentifier> SotaUptaneClient::getEcuHwId(const Uptane::EcuSerial &serial) const {
  if (serial == primary_ecu_serial_ || serial.ToString().empty()) {
    if (primary_ecu_hw_id_ == Uptane::HardwareIdentifier::Unknown()) {
      return boost::none;
    }
    return primary_ecu_hw_id_;
  }

  const auto it = secondaries.find(serial);
  if (it != secondaries.end()) {
    return it->second->getHwId();
  }

  return boost::none;
}
