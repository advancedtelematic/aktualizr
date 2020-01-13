#include "sotauptaneclient.h"

#include <fnmatch.h>
#include <unistd.h>
#include <memory>
#include <utility>

#include "campaign/campaign.h"
#include "crypto/crypto.h"
#include "crypto/keymanager.h"
#include "initializer.h"
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

void SotaUptaneClient::addNewSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec) {
  if (storage->loadEcuRegistered()) {
    EcuSerials serials;
    storage->loadEcuSerials(&serials);
    SerialCompare secondary_comp(sec->getSerial());
    if (std::find_if(serials.cbegin(), serials.cend(), secondary_comp) == serials.cend()) {
      throw std::logic_error("Add new secondaries for provisioned device is not implemented yet");
    }
  }
  addSecondary(sec);
}

void SotaUptaneClient::addSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec) {
  const Uptane::EcuSerial sec_serial = sec->getSerial();
  const Uptane::HardwareIdentifier sec_hw_id = sec->getHwId();
  std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface>>::const_iterator map_it =
      secondaries.find(sec_serial);
  if (map_it != secondaries.end()) {
    throw std::runtime_error(std::string("Multiple secondaries found with the same serial: ") + sec_serial.ToString());
  }
  secondaries.insert(std::make_pair(sec_serial, sec));
  hw_ids.insert(std::make_pair(sec_serial, sec_hw_id));
}

bool SotaUptaneClient::isInstalledOnPrimary(const Uptane::Target &target) {
  if (target.ecus().find(uptane_manifest.getPrimaryEcuSerial()) != target.ecus().end()) {
    return target.MatchTarget(package_manager_->getCurrent());
  }
  return false;
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
    return data::InstallationResult(data::ResultCode::Numeric::kInstallFailed, ex.what());
  }
}

void SotaUptaneClient::finalizeAfterReboot() {
  if (!package_manager_->rebootDetected()) {
    // nothing to do
    return;
  }

  LOG_INFO << "Device has been rebooted after an update";

  std::vector<Uptane::Target> updates;
  unsigned int ecus_count = 0;
  if (uptaneOfflineIteration(&updates, &ecus_count)) {
    const Uptane::EcuSerial &ecu_serial = uptane_manifest.getPrimaryEcuSerial();

    std::vector<Uptane::Target> installed_versions;
    boost::optional<Uptane::Target> pending_target;
    storage->loadInstalledVersions(ecu_serial.ToString(), nullptr, &pending_target);

    if (!!pending_target) {
      const std::string correlation_id = pending_target->correlation_id();

      data::InstallationResult install_res = package_manager_->finalizeInstall(*pending_target);
      storage->saveEcuInstallationResult(ecu_serial, install_res);
      if (install_res.success) {
        storage->saveInstalledVersion(ecu_serial.ToString(), *pending_target, InstalledVersionUpdateMode::kCurrent);
        report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(ecu_serial, correlation_id, true));
      } else {
        // finalize failed
        // unset pending flag so that the rest of the uptane process can
        // go forward again
        storage->saveInstalledVersion(ecu_serial.ToString(), *pending_target, InstalledVersionUpdateMode::kNone);
        report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(ecu_serial, correlation_id, false));
        director_repo.dropTargets(*storage);  // fix for OTA-2587, listen to backend again after end of install
      }

      computeDeviceInstallationResult(nullptr, correlation_id);
      putManifestSimple();
    } else {
      // nothing found on primary
      LOG_ERROR << "Expected reboot after update on primary but no update found";
    }
  } else {
    LOG_ERROR << "Invalid Uptane metadata in storage.";
  }

  package_manager_->rebootFlagClear();
}

data::InstallationResult SotaUptaneClient::PackageInstallSetResult(const Uptane::Target &target) {
  data::InstallationResult result;
  Uptane::EcuSerial ecu_serial = uptane_manifest.getPrimaryEcuSerial();

  // This is to recover more gracefully if the install process was interrupted
  // but ends up booting the new version anyway (e.g: ostree finished
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
    // ostree case: need reboot
    storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kPending);
  }
  storage->saveEcuInstallationResult(ecu_serial, result);
  return result;
}

void SotaUptaneClient::reportHwInfo() {
  Json::Value hw_info = Utils::getHardwareInfo();
  if (!hw_info.empty()) {
    if (hw_info != last_hw_info_reported) {
      if (http->put(config.tls.server + "/system_info", hw_info).isOk()) {
        last_hw_info_reported = hw_info;
      }
    }
  } else {
    LOG_WARNING << "Unable to fetch hardware information from host system.";
  }
}

void SotaUptaneClient::reportInstalledPackages() {
  http->put(config.tls.server + "/core/installed", package_manager_->getInstalledPackages());
}

void SotaUptaneClient::reportNetworkInfo() {
  if (config.telemetry.report_network) {
    LOG_DEBUG << "Reporting network information";
    Json::Value network_info = Utils::getNetworkInfo();
    if (network_info != last_network_info_reported) {
      HttpResponse response = http->put(config.tls.server + "/system_info/network", network_info);
      if (response.isOk()) {
        last_network_info_reported = network_info;
      }
    }

  } else {
    LOG_DEBUG << "Not reporting network information because telemetry is disabled";
  }
}

void SotaUptaneClient::reportAktualizrConfiguration() {
  if (!config.telemetry.report_config) {
    LOG_DEBUG << "Not reporting network information because telemetry is disabled";
    return;
  }

  LOG_DEBUG << "Reporting libaktualizr configuration";
  std::stringstream conf_ss;
  config.writeToStream(conf_ss);
  http->post(config.tls.server + "/system_info/config", "application/toml", conf_ss.str());
}

Json::Value SotaUptaneClient::AssembleManifest() {
  Json::Value manifest;  // signed top-level
  Uptane::EcuSerial primary_ecu_serial = uptane_manifest.getPrimaryEcuSerial();
  manifest["primary_ecu_serial"] = primary_ecu_serial.ToString();

  // first part: report current version/state of all ecus
  Json::Value version_manifest;

  Json::Value primary_ecu_version = package_manager_->getManifest(primary_ecu_serial);
  std::vector<std::pair<Uptane::EcuSerial, int64_t>> ecu_cnt;
  if (!storage->loadEcuReportCounter(&ecu_cnt) || (ecu_cnt.size() == 0)) {
    LOG_ERROR << "No ECU version report counter, please check the database!";
  } else {
    primary_ecu_version["report_counter"] = std::to_string(ecu_cnt[0].second + 1);
    storage->saveEcuReportCounter(ecu_cnt[0].first, ecu_cnt[0].second + 1);
  }
  version_manifest[primary_ecu_serial.ToString()] = uptane_manifest.signManifest(primary_ecu_version);

  for (auto it = secondaries.begin(); it != secondaries.end(); it++) {
    Json::Value secmanifest = it->second->getManifest();
    if (secmanifest.isMember("signatures") && secmanifest.isMember("signed")) {
      const auto public_key = it->second->getPublicKey();
      const std::string canonical = Utils::jsonToCanonicalStr(secmanifest["signed"]);
      const bool verified = public_key.VerifySignature(secmanifest["signatures"][0]["sig"].asString(), canonical);

      if (verified) {
        version_manifest[it->first.ToString()] = secmanifest;
      } else {
        LOG_ERROR << "Secondary manifest verification failed, manifest: " << secmanifest;
      }
    } else {
      LOG_ERROR << "Secondary manifest is corrupted or not signed, manifest: " << secmanifest;
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
  KeyManager keys(storage, config.keymanagerConfig());
  Initializer initializer(config.provision, storage, http, keys, secondaries);

  if (!initializer.isSuccessful()) {
    throw std::runtime_error("Fatal error during provisioning or ECU device registration.");
  }

  EcuSerials serials;
  if (!storage->loadEcuSerials(&serials) || serials.size() == 0) {
    throw std::runtime_error("Unable to load ECU serials after device registration.");
  }

  uptane_manifest.setPrimaryEcuSerialHwId(serials[0]);
  hw_ids.insert(serials[0]);

  verifySecondaries();
  LOG_DEBUG << "... provisioned OK";

  finalizeAfterReboot();
}

bool SotaUptaneClient::updateDirectorMeta() {
  if (!director_repo.updateMeta(*storage, *uptane_fetcher)) {
    last_exception = director_repo.getLastException();
    return false;
  }
  return true;
}

bool SotaUptaneClient::updateImagesMeta() {
  if (!images_repo.updateMeta(*storage, *uptane_fetcher)) {
    last_exception = images_repo.getLastException();
    return false;
  }
  return true;
}

bool SotaUptaneClient::checkDirectorMetaOffline() {
  if (!director_repo.checkMetaOffline(*storage)) {
    last_exception = director_repo.getLastException();
    return false;
  }
  return true;
}

bool SotaUptaneClient::checkImagesMetaOffline() {
  if (!images_repo.checkMetaOffline(*storage)) {
    last_exception = images_repo.getLastException();
    return false;
  }
  return true;
}

void SotaUptaneClient::computeDeviceInstallationResult(data::InstallationResult *result,
                                                       const std::string &correlation_id) {
  data::InstallationResult device_installation_result =
      data::InstallationResult(data::ResultCode::Numeric::kOk, "Device has been successfully installed");
  std::string raw_installation_report = "Installation succesful";

  do {
    std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_results;

    if (!storage->loadEcuInstallationResults(&ecu_results)) {
      // failed to load ECUs' installation result
      device_installation_result = data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                                            "Unable to get installation results from ecus");
      raw_installation_report = "Failed to load ECUs' installation result";

      break;
    }

    std::string result_code_err_str;

    for (const auto &r : ecu_results) {
      auto ecu_serial = r.first;
      auto installation_res = r.second;

      if (hw_ids.find(ecu_serial) == hw_ids.end()) {
        // couldn't find any ECU with the given serial/ID
        device_installation_result = data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                                              "Unable to get installation results from ecus");

        raw_installation_report = "Couldn't find any ECU with the given serial: " + ecu_serial.ToString();

        break;
      }

      if (installation_res.needCompletion()) {
        // one of the ECUs needs completion, aka an installation finalization
        device_installation_result =
            data::InstallationResult(data::ResultCode::Numeric::kNeedCompletion,
                                     "ECU needs completion/finalization to be installed: " + ecu_serial.ToString());
        raw_installation_report = "ECU needs completion/finalization to be installed: " + ecu_serial.ToString();

        break;
      }

      // format:
      // ecu1_hwid:failure1|ecu2_hwid:failure2
      if (!installation_res.isSuccess()) {
        std::string ecu_code_str = hw_ids.at(ecu_serial).ToString() + ":" + installation_res.result_code.toString();
        result_code_err_str += (result_code_err_str != "" ? "|" : "") + ecu_code_str;
      }
    }

    if (!result_code_err_str.empty()) {
      // installation on at least one of the ECUs has failed
      device_installation_result =
          data::InstallationResult(data::ResultCode(data::ResultCode::Numeric::kInstallFailed, result_code_err_str),
                                   "Installation failed on at least one of ECUs");
      raw_installation_report = "Installation failed on at least one of ECUs";

      break;
    }

  } while (false);

  if (result != nullptr) {
    *result = device_installation_result;
  }

  // TODO: think of exception handling,  the SQLite related code can throw exceptions
  storage->storeDeviceInstallationResult(device_installation_result, raw_installation_report, correlation_id);
}

bool SotaUptaneClient::getNewTargets(std::vector<Uptane::Target> *new_targets, unsigned int *ecus_count) {
  std::vector<Uptane::Target> targets = director_repo.getTargets().targets;
  Uptane::EcuSerial primary_ecu_serial = uptane_manifest.getPrimaryEcuSerial();
  if (ecus_count != nullptr) {
    *ecus_count = 0;
  }
  for (const Uptane::Target &target : targets) {
    bool is_new = false;
    for (const auto &ecu : target.ecus()) {
      Uptane::EcuSerial ecu_serial = ecu.first;
      Uptane::HardwareIdentifier hw_id = ecu.second;

      auto hwid_it = hw_ids.find(ecu_serial);
      if (hwid_it == hw_ids.end()) {
        LOG_ERROR << "Unknown ECU ID in director targets metadata: " << ecu_serial.ToString();
        last_exception = Uptane::BadEcuId(target.filename());
        return false;
      }

      if (hwid_it->second != hw_id) {
        LOG_ERROR << "Wrong hardware identifier for ECU " << ecu_serial.ToString();
        last_exception = Uptane::BadHardwareId(target.filename());
        return false;
      }

      boost::optional<Uptane::Target> current_version;
      if (!storage->loadInstalledVersions(ecu_serial.ToString(), &current_version, nullptr)) {
        LOG_WARNING << "Could not load currently installed version for ECU ID: " << ecu_serial.ToString();
        break;
      }

      if (!current_version) {
        LOG_WARNING << "Current version for ECU ID: " << ecu_serial.ToString() << " is unknown";
        is_new = true;
      } else if (current_version->filename() != target.filename()) {
        is_new = true;
      }

      if (primary_ecu_serial == ecu_serial) {
        if (!target.IsOstree() &&
            (config.pacman.type == PackageManager::kOstree || config.pacman.type == PackageManager::kOstreeDockerApp)) {
          LOG_ERROR << "Cannot install a non-OSTree package on an OSTree system";
          last_exception = Uptane::InvalidTarget(target.filename());
          return false;
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
  return true;
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
        Uptane::getTrustedDelegation(delegate_role, cur_targets, images_repo, *storage, *uptane_fetcher, offline);
    if (delegation.isExpired(TimeStamp::Now())) {
      continue;
    }

    auto is_terminating = cur_targets.terminating_role_.find(delegate_role);
    if (is_terminating == cur_targets.terminating_role_.end()) {
      throw Uptane::Exception("images", "Inconsistent delegations");
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
  auto toplevel_targets = images_repo.getTargets();
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

  result::UpdateStatus update_status = checkUpdatesOffline(targets);
  if (update_status != result::UpdateStatus::kUpdatesAvailable) {
    if (update_status == result::UpdateStatus::kNoUpdatesAvailable) {
      result = result::Download({}, result::DownloadStatus::kNothingToDownload, "");
    } else {
      result =
          result::Download(downloaded_targets, result::DownloadStatus::kError, "Error rechecking stored metadata.");
      storeInstallationFailure(
          data::InstallationResult(data::ResultCode::Numeric::kInternalError, "Error rechecking stored metadata."));
    }
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
    if (downloaded_targets.size() == 0) {
      LOG_ERROR << "None of " << targets.size() << " targets were successfully downloaded.";
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
  // TODO: support downloading encrypted targets from director

  const std::string &correlation_id = director_repo.getCorrelationId();
  // send an event for all ecus that are touched by this target
  for (const auto &ecu : target.ecus()) {
    report_queue->enqueue(std_::make_unique<EcuDownloadStartedReport>(ecu.first, correlation_id));
  }

  Uptane::EcuSerial primary_ecu_serial = uptane_manifest.getPrimaryEcuSerial();

  bool success = false;
  if (target.IsForEcu(primary_ecu_serial) || !target.IsOstree()) {
    // TODO: download should be the logical ECU and packman specific
    KeyManager keys(storage, config.keymanagerConfig());
    keys.loadKeys();
    auto prog_cb = [this](const Uptane::Target &t, const std::string description, unsigned int progress) {
      report_progress_cb(events_channel.get(), t, description, progress);
    };

    const int max_tries = 3;
    int tries = 0;
    std::chrono::milliseconds wait(500);

    for (; tries < max_tries; tries++) {
      success = package_manager_->fetchTarget(target, *uptane_fetcher, keys, prog_cb, token);
      if (success) {
        break;
      } else if (tries < max_tries - 1) {
        std::this_thread::sleep_for(wait);
        wait *= 2;
      }
    }
    if (!success) {
      LOG_ERROR << "Download unsuccessful after " << tries << " attempts.";
    }
  } else {
    // we emulate successfull download in case of the secondary ostree update
    success = true;
  }

  // send this asynchronously before `sendEvent`, so that the report timestamp
  // would not be delayed by callbacks on events
  for (const auto &ecu : target.ecus()) {
    report_queue->enqueue(std_::make_unique<EcuDownloadCompletedReport>(ecu.first, correlation_id, success));
  }

  sendEvent<event::DownloadTargetComplete>(target, success);
  return {success, target};
}

bool SotaUptaneClient::uptaneIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count) {
  if (!updateDirectorMeta()) {
    LOG_ERROR << "Failed to update director metadata: " << last_exception.what();
    return false;
  }
  std::vector<Uptane::Target> tmp_targets;
  unsigned int ecus;
  if (!getNewTargets(&tmp_targets, &ecus)) {
    LOG_ERROR << "Inconsistency between director metadata and existent ECUs";
    return false;
  }

  if (tmp_targets.empty()) {
    if (targets != nullptr) {
      *targets = std::move(tmp_targets);
    }
    if (ecus_count != nullptr) {
      *ecus_count = ecus;
    }
    return true;
  }

  LOG_INFO << "got new updates";

  if (!updateImagesMeta()) {
    LOG_ERROR << "Failed to update images metadata: " << last_exception.what();
    return false;
  }

  if (targets != nullptr) {
    *targets = std::move(tmp_targets);
  }
  if (ecus_count != nullptr) {
    *ecus_count = ecus;
  }
  return true;
}

bool SotaUptaneClient::uptaneOfflineIteration(std::vector<Uptane::Target> *targets, unsigned int *ecus_count) {
  if (!checkDirectorMetaOffline()) {
    LOG_ERROR << "Failed to check director metadata: " << last_exception.what();
    return false;
  }
  std::vector<Uptane::Target> tmp_targets;
  unsigned int ecus;
  if (!getNewTargets(&tmp_targets, &ecus)) {
    LOG_ERROR << "Inconsistency between director metadata and existent ECUs";
    return false;
  }

  if (tmp_targets.empty()) {
    *targets = std::move(tmp_targets);
    if (ecus_count != nullptr) {
      *ecus_count = ecus;
    }
    return true;
  }

  if (!checkImagesMetaOffline()) {
    LOG_ERROR << "Failed to check images metadata: " << last_exception.what();
    return false;
  }

  *targets = std::move(tmp_targets);
  if (ecus_count != nullptr) {
    *ecus_count = ecus;
  }
  return true;
}

void SotaUptaneClient::sendDeviceData() {
  reportHwInfo();
  reportInstalledPackages();
  reportNetworkInfo();
  reportAktualizrConfiguration();
  putManifestSimple();
  sendEvent<event::SendDeviceDataComplete>();
}

result::UpdateCheck SotaUptaneClient::fetchMeta() {
  result::UpdateCheck result;

  reportNetworkInfo();

  if (hasPendingUpdates()) {
    LOG_INFO << "An update is pending. Check if pending ECUs has been already updated";

    std::vector<std::pair<Uptane::EcuSerial, Uptane::Hash>> pending_ecus;
    storage->getPendingEcus(&pending_ecus);
    const auto &primary_ecu_serial = uptane_manifest.getPrimaryEcuSerial();

    for (const auto &pending_ecu : pending_ecus) {
      if (primary_ecu_serial == pending_ecu.first) {
        continue;
      }
      auto &sec = secondaries[pending_ecu.first];
      auto manifest = sec->getManifest();
      const std::string man_body = Utils::jsonToCanonicalStr(manifest["signed"]);
      auto current_ecu_hash = manifest["signed"]["installed_image"]["fileinfo"]["hashes"]["sha256"];
      LOG_INFO << "Current secondary hash: " << current_ecu_hash;
      LOG_INFO << "Pending secondary hash: " << pending_ecu.second.HashString();

      if (pending_ecu.second == Uptane::Hash(Uptane::Hash::Type::kSha256, current_ecu_hash.asString())) {
        LOG_INFO << "The pending update has been installed: " << current_ecu_hash;
        boost::optional<Uptane::Target> pending_version;
        if (storage->loadInstalledVersions(pending_ecu.first.ToString(), nullptr, &pending_version)) {
          storage->saveEcuInstallationResult(pending_ecu.first,
                                             data::InstallationResult(data::ResultCode::Numeric::kOk, ""));

          storage->saveInstalledVersion(pending_ecu.first.ToString(), *pending_version,
                                        InstalledVersionUpdateMode::kCurrent);

          report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(
              pending_ecu.first, director_repo.getCorrelationId(), true));
          computeDeviceInstallationResult(nullptr, director_repo.getCorrelationId());
        }
      }
    }
  }

  if (hasPendingUpdates()) {
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
  if (!uptaneIteration(&updates, &ecus_count)) {
    result = result::UpdateCheck({}, 0, result::UpdateStatus::kError, Json::nullValue, "Could not update metadata.");
    return result;
  }

  std::string director_targets;
  storage->loadNonRoot(&director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());

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
  for (auto &target : updates) {
    auto images_target = findTargetInDelegationTree(target, false);
    if (images_target == nullptr) {
      // TODO: Could also be a missing target or delegation expiration.
      last_exception = Uptane::TargetMismatch(target.filename());
      LOG_ERROR << "No matching target in images targets metadata for " << target;
      result = result::UpdateCheck({}, 0, result::UpdateStatus::kError, Utils::parseJSON(director_targets),
                                   "Target mismatch.");
      storeInstallationFailure(
          data::InstallationResult(data::ResultCode::Numeric::kVerificationFailed, "Metadata verification failed."));
      return result;
    }
    // If the URL from the Director is unset, but the URL from the Images repo
    // is set, use that.
    if (target.uri().empty() && !images_target->uri().empty()) {
      target.setUri(images_target->uri());
    }
  }

  result = result::UpdateCheck(updates, ecus_count, result::UpdateStatus::kUpdatesAvailable,
                               Utils::parseJSON(director_targets), "");
  if (updates.size() == 1) {
    LOG_INFO << "1 new update found in Uptane metadata.";
  } else {
    LOG_INFO << updates.size() << " new updates found in Uptane metadata.";
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
  if (!uptaneOfflineIteration(&director_targets, &ecus_count)) {
    LOG_ERROR << "Invalid Uptane metadata in storage.";
    return result::UpdateStatus::kError;
  }

  if (director_targets.empty()) {
    LOG_ERROR << "No new updates found in Uptane metadata, but expected " << targets.size() << ".";
    return result::UpdateStatus::kNoUpdatesAvailable;
  }

  // For every target in the Director Targets metadata, walk the delegation
  // tree (if necessary) and find a matching target in the Images repo
  // metadata.
  for (const auto &target : targets) {
    TargetCompare target_comp(target);
    const auto it = std::find_if(director_targets.cbegin(), director_targets.cend(), target_comp);
    if (it == director_targets.cend()) {
      LOG_ERROR << "No matching target in director targets metadata for " << target;
      return result::UpdateStatus::kError;
    }

    const auto images_target = findTargetInDelegationTree(target, true);
    if (images_target == nullptr) {
      LOG_ERROR << "No matching target in images targets metadata for " << target;
      return result::UpdateStatus::kError;
    }
  }

  return result::UpdateStatus::kUpdatesAvailable;
}

result::Install SotaUptaneClient::uptaneInstall(const std::vector<Uptane::Target> &updates) {
  result::Install result;
  const std::string &correlation_id = director_repo.getCorrelationId();

  // clear all old results first
  storage->clearInstallationResults();

  // Recheck the Uptane metadata and make sure the requested updates are
  // consistent with the stored metadata.
  result::UpdateStatus update_status = checkUpdatesOffline(updates);
  if (update_status != result::UpdateStatus::kUpdatesAvailable) {
    if (update_status == result::UpdateStatus::kNoUpdatesAvailable) {
      result.dev_report = {false, data::ResultCode::Numeric::kAlreadyProcessed, ""};
    } else {
      result.dev_report = {false, data::ResultCode::Numeric::kInternalError, ""};
    }
    storage->storeDeviceInstallationResult(result.dev_report, "Stored Uptane metadata is invalid", correlation_id);
    sendEvent<event::AllInstallsComplete>(result);
    return result;
  }

  Uptane::EcuSerial primary_ecu_serial = uptane_manifest.getPrimaryEcuSerial();

  // Recheck the downloaded update hashes.
  for (const auto &update : updates) {
    if (update.IsForEcu(primary_ecu_serial) || !update.IsOstree()) {
      if (package_manager_->verifyTarget(update) != TargetStatus::kGood) {
        result.dev_report = {false, data::ResultCode::Numeric::kInternalError, ""};
        storage->storeDeviceInstallationResult(result.dev_report, "Downloaded target is invalid", correlation_id);
        sendEvent<event::AllInstallsComplete>(result);
        return result;
      }
    }
  }

  // Uptane step 5 (send time to all ECUs) is not implemented yet.

  std::vector<Uptane::Target> primary_updates = findForEcu(updates, primary_ecu_serial);
  //   6 - send metadata to all the ECUs
  sendMetadataToEcus(updates);

  //   7 - send images to ECUs (deploy for OSTree)
  if (primary_updates.size() != 0u) {
    // assuming one OSTree OS per primary => there can be only one OSTree update
    Uptane::Target primary_update = primary_updates[0];
    primary_update.setCorrelationId(correlation_id);

    report_queue->enqueue(std_::make_unique<EcuInstallationStartedReport>(primary_ecu_serial, correlation_id));
    sendEvent<event::InstallStarted>(primary_ecu_serial);

    data::InstallationResult install_res;
    if (!isInstalledOnPrimary(primary_update)) {
      // notify the bootloader before installation happens, because installation is not atomic and
      //   a false notification doesn't hurt when rollbacks are implemented
      package_manager_->updateNotify();
      install_res = PackageInstallSetResult(primary_update);
      if (install_res.result_code.num_code == data::ResultCode::Numeric::kNeedCompletion) {
        // update needs a reboot, send distinct EcuInstallationApplied event
        report_queue->enqueue(std_::make_unique<EcuInstallationAppliedReport>(primary_ecu_serial, correlation_id));
        sendEvent<event::InstallTargetComplete>(primary_ecu_serial, true);  // TODO: distinguish from success here?
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
    } else {
      install_res = data::InstallationResult(data::ResultCode::Numeric::kAlreadyProcessed, "Package already installed");
      storage->saveEcuInstallationResult(primary_ecu_serial, install_res);
      // TODO: distinguish this case from regular failure for local and remote
      // event reporting
      report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(uptane_manifest.getPrimaryEcuSerial(),
                                                                              correlation_id, false));
      sendEvent<event::InstallTargetComplete>(uptane_manifest.getPrimaryEcuSerial(), false);
    }
    result.ecu_reports.emplace(result.ecu_reports.begin(), primary_update, uptane_manifest.getPrimaryEcuSerial(),
                               install_res);
    // TODO: other updates for primary
  } else {
    LOG_INFO << "No update to install on primary";
  }

  auto sec_reports = sendImagesToEcus(updates);
  result.ecu_reports.insert(result.ecu_reports.end(), sec_reports.begin(), sec_reports.end());
  computeDeviceInstallationResult(&result.dev_report, correlation_id);
  sendEvent<event::AllInstallsComplete>(result);

  if (!(result.dev_report.isSuccess() || result.dev_report.needCompletion())) {
    director_repo.dropTargets(*storage);  // fix for OTA-2587, listen to backend again after end of install
  }

  return result;
}

result::CampaignCheck SotaUptaneClient::campaignCheck() {
  auto campaigns = campaign::fetchAvailableCampaigns(*http, config.tls.server);
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
  bool force_install_completion = (hasPendingUpdates() && config.uptane.force_install_completion);
  return force_install_completion;
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
  if (custom != Json::nullValue) {
    manifest["custom"] = custom;
  }
  auto signed_manifest = uptane_manifest.signManifest(manifest);
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

// Check stored secondaries list against secondaries known to aktualizr.
void SotaUptaneClient::verifySecondaries() {
  storage->clearMisconfiguredEcus();
  EcuSerials serials;
  if (!storage->loadEcuSerials(&serials) || serials.empty()) {
    LOG_ERROR << "No ECU serials found in storage!";
    return;
  }

  std::vector<MisconfiguredEcu> misconfigured_ecus;
  std::vector<bool> found(serials.size(), false);
  SerialCompare primary_comp(uptane_manifest.getPrimaryEcuSerial());
  EcuSerials::const_iterator store_it;
  store_it = std::find_if(serials.cbegin(), serials.cend(), primary_comp);
  if (store_it == serials.cend()) {
    LOG_ERROR << "Primary ECU serial " << uptane_manifest.getPrimaryEcuSerial() << " not found in storage!";
    misconfigured_ecus.emplace_back(uptane_manifest.getPrimaryEcuSerial(), Uptane::HardwareIdentifier(""),
                                    EcuState::kOld);
  } else {
    found[static_cast<size_t>(std::distance(serials.cbegin(), store_it))] = true;
  }

  std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface>>::const_iterator it;
  for (it = secondaries.cbegin(); it != secondaries.cend(); ++it) {
    SerialCompare secondary_comp(it->second->getSerial());
    store_it = std::find_if(serials.cbegin(), serials.cend(), secondary_comp);
    if (store_it == serials.cend()) {
      LOG_ERROR << "Secondary ECU serial " << it->second->getSerial() << " (hardware ID " << it->second->getHwId()
                << ") not found in storage!";
      misconfigured_ecus.emplace_back(it->second->getSerial(), it->second->getHwId(), EcuState::kNotRegistered);
    } else if (found[static_cast<size_t>(std::distance(serials.cbegin(), store_it))]) {
      LOG_ERROR << "Secondary ECU serial " << it->second->getSerial() << " (hardware ID " << it->second->getHwId()
                << ") has a duplicate entry in storage!";
    } else {
      found[static_cast<size_t>(std::distance(serials.cbegin(), store_it))] = true;
    }
  }

  std::vector<bool>::iterator found_it;
  for (found_it = found.begin(); found_it != found.end(); ++found_it) {
    if (!*found_it) {
      auto not_registered = serials[static_cast<size_t>(std::distance(found.begin(), found_it))];
      LOG_WARNING << "ECU serial " << not_registered.first << " in storage was not reported to aktualizr!";
      misconfigured_ecus.emplace_back(not_registered.first, not_registered.second, EcuState::kOld);
    }
  }

  storage->storeMisconfiguredEcus(misconfigured_ecus);
}

void SotaUptaneClient::storeInstallationFailure(const data::InstallationResult &result) {
  // Store installation report to inform Director of the update failure before
  // we actually got to the install step.
  const std::string &correlation_id = director_repo.getCorrelationId();
  storage->storeDeviceInstallationResult(result, "", correlation_id);
  // Fix for OTA-2587, listen to backend again after end of install.
  director_repo.dropTargets(*storage);
}

void SotaUptaneClient::rotateSecondaryRoot(Uptane::RepositoryType repo, Uptane::SecondaryInterface &secondary) {
  std::string latest_root;

  if (!storage->loadLatestRoot(&latest_root, repo)) {
    LOG_ERROR << "No root metadata to send";
    return;
  }

  int last_root_version = Uptane::extractVersionUntrusted(latest_root);

  int sec_root_version = secondary.getRootVersion((repo == Uptane::RepositoryType::Director()));
  if (sec_root_version >= 0) {
    for (int v = sec_root_version + 1; v <= last_root_version; v++) {
      std::string root;
      if (!storage->loadRoot(&root, repo, Uptane::Version(v))) {
        LOG_WARNING << "Couldn't find root meta in the storage, trying remote repo";
        if (!uptane_fetcher->fetchRole(&root, Uptane::kMaxRootSize, repo, Uptane::Role::Root(), Uptane::Version(v))) {
          // TODO: looks problematic, robust procedure needs to be defined
          LOG_ERROR << "Root metadata could not be fetched, skipping to the next secondary";
          return;
        }
      }
      if (!secondary.putRoot(root, repo == Uptane::RepositoryType::Director())) {
        LOG_ERROR << "Sending metadata to " << secondary.getSerial() << " failed";
      }
    }
  }
}

// TODO: the function can't currently return any errors. The problem of error reporting from secondaries should
// be solved on a system (backend+frontend) error.
// TODO: the function blocks until it updates all the secondaries. Consider non-blocking operation.
void SotaUptaneClient::sendMetadataToEcus(const std::vector<Uptane::Target> &targets) {
  Uptane::RawMetaPack meta;
  if (!storage->loadLatestRoot(&meta.director_root, Uptane::RepositoryType::Director())) {
    LOG_ERROR << "No director root metadata to send";
    return;
  }
  if (!storage->loadNonRoot(&meta.director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets())) {
    LOG_ERROR << "No director targets metadata to send";
    return;
  }
  if (!storage->loadLatestRoot(&meta.image_root, Uptane::RepositoryType::Image())) {
    LOG_ERROR << "No images root metadata to send";
    return;
  }
  if (!storage->loadNonRoot(&meta.image_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp())) {
    LOG_ERROR << "No images timestamp metadata to send";
    return;
  }
  if (!storage->loadNonRoot(&meta.image_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot())) {
    LOG_ERROR << "No images snapshot metadata to send";
    return;
  }
  if (!storage->loadNonRoot(&meta.image_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets())) {
    LOG_ERROR << "No images targets metadata to send";
    return;
  }

  // target images should already have been downloaded to metadata_path/targets/
  for (auto targets_it = targets.cbegin(); targets_it != targets.cend(); ++targets_it) {
    for (auto ecus_it = targets_it->ecus().cbegin(); ecus_it != targets_it->ecus().cend(); ++ecus_it) {
      const Uptane::EcuSerial ecu_serial = ecus_it->first;

      auto sec = secondaries.find(ecu_serial);
      if (sec != secondaries.end()) {
        /* Root rotation if necessary */
        rotateSecondaryRoot(Uptane::RepositoryType::Director(), *(sec->second));
        rotateSecondaryRoot(Uptane::RepositoryType::Image(), *(sec->second));
        if (!sec->second->putMetadata(meta)) {
          LOG_ERROR << "Sending metadata to " << sec->second->getSerial() << " failed";
        }
      }
    }
  }
}

std::future<data::ResultCode::Numeric> SotaUptaneClient::sendFirmwareAsync(Uptane::SecondaryInterface &secondary,
                                                                           const std::shared_ptr<std::string> &data,
                                                                           const std::string &filename) {
  auto f = [this, &secondary, data, filename]() {
    const std::string &correlation_id = director_repo.getCorrelationId();
    sendEvent<event::InstallStarted>(secondary.getSerial());
    report_queue->enqueue(std_::make_unique<EcuInstallationStartedReport>(secondary.getSerial(), correlation_id));

    bool send_firmware_result = secondary.sendFirmware(data);
    data::ResultCode::Numeric result =
        send_firmware_result ? data::ResultCode::Numeric::kOk : data::ResultCode::Numeric::kInstallFailed;

    if (send_firmware_result) {
      result = secondary.install(filename);
    }

    if (result == data::ResultCode::Numeric::kNeedCompletion) {
      report_queue->enqueue(std_::make_unique<EcuInstallationAppliedReport>(secondary.getSerial(), correlation_id));
    } else {
      report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(
          secondary.getSerial(), correlation_id, result == data::ResultCode::Numeric::kOk));
    }
    sendEvent<event::InstallTargetComplete>(secondary.getSerial(), result == data::ResultCode::Numeric::kOk);
    return result;
  };

  return std::async(std::launch::async, f);
}

std::vector<result::Install::EcuReport> SotaUptaneClient::sendImagesToEcus(const std::vector<Uptane::Target> &targets) {
  std::vector<result::Install::EcuReport> reports;
  std::vector<std::pair<result::Install::EcuReport, std::future<data::ResultCode::Numeric>>> firmwareFutures;

  Uptane::EcuSerial primary_ecu_serial = uptane_manifest.getPrimaryEcuSerial();
  // target images should already have been downloaded to metadata_path/targets/
  for (auto targets_it = targets.cbegin(); targets_it != targets.cend(); ++targets_it) {
    for (auto ecus_it = targets_it->ecus().cbegin(); ecus_it != targets_it->ecus().cend(); ++ecus_it) {
      const Uptane::EcuSerial &ecu_serial = ecus_it->first;

      if (primary_ecu_serial == ecu_serial) {
        continue;
      }

      auto f = secondaries.find(ecu_serial);
      if (f == secondaries.end()) {
        LOG_ERROR << "Target " << *targets_it << " has unknown ECU ID";
        last_exception = Uptane::BadEcuId(targets_it->filename());
        continue;
      }
      Uptane::SecondaryInterface &sec = *f->second;

      if (targets_it->IsOstree()) {
        // empty firmware means OSTree secondaries: pack credentials instead
        const std::string creds_archive = secondaryTreehubCredentials();
        if (creds_archive.empty()) {
          continue;
        }
        firmwareFutures.emplace_back(
            result::Install::EcuReport(*targets_it, ecu_serial, data::InstallationResult()),
            sendFirmwareAsync(sec, std::make_shared<std::string>(creds_archive), (*targets_it).filename()));
      } else {
        std::stringstream sstr;
        sstr << *storage->openTargetFile(*targets_it);
        const std::string fw = sstr.str();
        firmwareFutures.emplace_back(
            result::Install::EcuReport(*targets_it, ecu_serial, data::InstallationResult()),
            sendFirmwareAsync(sec, std::make_shared<std::string>(fw), (*targets_it).filename()));
      }
    }
  }

  for (auto &f : firmwareFutures) {
    // failure
    if (fiu_fail((std::string("secondary_install_") + f.first.serial.ToString()).c_str()) != 0) {
      f.first.install_res = data::InstallationResult(
          data::ResultCode(data::ResultCode::Numeric::kInstallFailed, fault_injection_last_info()), "");
      storage->saveEcuInstallationResult(f.first.serial, f.first.install_res);
      reports.push_back(f.first);
      continue;
    }

    auto fut_result = f.second.get();
    f.first.install_res = data::InstallationResult(fut_result, "");
    if (fut_result == data::ResultCode::Numeric::kOk) {
      storage->saveInstalledVersion(f.first.serial.ToString(), f.first.update, InstalledVersionUpdateMode::kCurrent);
    } else if (fut_result == data::ResultCode::Numeric::kNeedCompletion) {
      // ostree case: need reboot
      storage->saveInstalledVersion(f.first.serial.ToString(), f.first.update, InstalledVersionUpdateMode::kPending);
    }

    storage->saveEcuInstallationResult(f.first.serial, f.first.install_res);
    reports.push_back(f.first);
  }
  return reports;
}

std::string SotaUptaneClient::secondaryTreehubCredentials() const {
  if (config.tls.pkey_source != CryptoSource::kFile || config.tls.cert_source != CryptoSource::kFile ||
      config.tls.ca_source != CryptoSource::kFile) {
    LOG_ERROR << "Cannot send OSTree update to a secondary when not using file as credential sources";
    return "";
  }
  std::string ca, cert, pkey;
  if (!storage->loadTlsCreds(&ca, &cert, &pkey)) {
    LOG_ERROR << "Could not load tls credentials from storage";
    return "";
  }

  std::string treehub_url = config.pacman.ostree_server;
  std::map<std::string, std::string> archive_map = {
      {"ca.pem", ca}, {"client.pem", cert}, {"pkey.pem", pkey}, {"server.url", treehub_url}};

  try {
    std::stringstream as;
    Utils::writeArchive(archive_map, as);

    return as.str();
  } catch (std::runtime_error &exc) {
    LOG_ERROR << "Could not create credentials archive: " << exc.what();
    return "";
  }
}

Uptane::LazyTargetsList SotaUptaneClient::allTargets() const {
  return Uptane::LazyTargetsList(images_repo, storage, uptane_fetcher);
}
