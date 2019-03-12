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
#include "package_manager/packagemanagerfactory.h"
#include "uptane/exceptions.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryfactory.h"
#include "utilities/utils.h"

static void report_progress_cb(event::Channel *channel, const Uptane::Target &target, const std::string &description,
                               unsigned int progress) {
  if (channel == nullptr) {
    return;
  }
  auto event = std::make_shared<event::DownloadProgressReport>(target, description, progress);
  (*channel)(event);
}

std::shared_ptr<SotaUptaneClient> SotaUptaneClient::newDefaultClient(
    Config &config_in, std::shared_ptr<INvStorage> storage_in, std::shared_ptr<event::Channel> events_channel_in) {
  std::shared_ptr<HttpClient> http_client_in = std::make_shared<HttpClient>();
  std::shared_ptr<Bootloader> bootloader_in = std::make_shared<Bootloader>(config_in.bootloader, *storage_in);
  std::shared_ptr<ReportQueue> report_queue_in = std::make_shared<ReportQueue>(config_in, http_client_in);

  return std::make_shared<SotaUptaneClient>(config_in, storage_in, http_client_in, bootloader_in, report_queue_in,
                                            events_channel_in);
}

std::shared_ptr<SotaUptaneClient> SotaUptaneClient::newTestClient(Config &config_in,
                                                                  std::shared_ptr<INvStorage> storage_in,
                                                                  std::shared_ptr<HttpInterface> http_client_in,
                                                                  std::shared_ptr<event::Channel> events_channel_in) {
  std::shared_ptr<Bootloader> bootloader_in = std::make_shared<Bootloader>(config_in.bootloader, *storage_in);
  std::shared_ptr<ReportQueue> report_queue_in = std::make_shared<ReportQueue>(config_in, http_client_in);
  return std::make_shared<SotaUptaneClient>(config_in, storage_in, http_client_in, bootloader_in, report_queue_in,
                                            events_channel_in);
}

SotaUptaneClient::SotaUptaneClient(Config &config_in, std::shared_ptr<INvStorage> storage_in,
                                   std::shared_ptr<HttpInterface> http_client,
                                   std::shared_ptr<Bootloader> bootloader_in,
                                   std::shared_ptr<ReportQueue> report_queue_in,
                                   std::shared_ptr<event::Channel> events_channel_in)
    : config(config_in),
      uptane_manifest(config, storage_in),
      storage(std::move(storage_in)),
      http(std::move(http_client)),
      bootloader(std::move(bootloader_in)),
      report_queue(std::move(report_queue_in)),
      events_channel(std::move(events_channel_in)) {
  // translate progress reports from the fetcher to actual API events
  auto prog_cb = [this](const Uptane::Target &target, const std::string description, unsigned int progress) {
    report_progress_cb(events_channel.get(), target, description, progress);
  };

  uptane_fetcher = std::make_shared<Uptane::Fetcher>(config, storage, http, prog_cb);

  // consider boot successful as soon as we started, missing internet connection or connection to secondaries are not
  // proper reasons to roll back
  package_manager_ = PackageManagerFactory::makePackageManager(config.pacman, storage, bootloader, http);
  if (package_manager_->imageUpdated()) {
    bootloader->setBootOK();
  }

  if (config.discovery.ipuptane) {
    IpSecondaryDiscovery ip_uptane_discovery{config.network};
    auto ipuptane_secs = ip_uptane_discovery.discover();
    config.uptane.secondary_configs.insert(config.uptane.secondary_configs.end(), ipuptane_secs.begin(),
                                           ipuptane_secs.end());
  }
  std::vector<Uptane::SecondaryConfig>::const_iterator it;
  for (it = config.uptane.secondary_configs.begin(); it != config.uptane.secondary_configs.end(); ++it) {
    auto sec = Uptane::SecondaryFactory::makeSecondary(*it);
    addSecondary(sec);
  }
}

SotaUptaneClient::~SotaUptaneClient() { conn.disconnect(); }

void SotaUptaneClient::addNewSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec) {
  if (storage->loadEcuRegistered()) {
    EcuSerials serials;
    storage->loadEcuSerials(&serials);
    SerialCompare secondary_comp(sec->getSerial());
    if (std::find_if(serials.begin(), serials.end(), secondary_comp) == serials.end()) {
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
    return target == package_manager_->getCurrent();
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
  if (!bootloader->rebootDetected()) {
    // nothing to do
    return;
  }

  LOG_INFO << "Device has been rebooted after an update";

  std::vector<Uptane::Target> updates;
  unsigned int ecus_count = 0;
  if (uptaneOfflineIteration(&updates, &ecus_count)) {
    const Uptane::EcuSerial &ecu_serial = uptane_manifest.getPrimaryEcuSerial();

    std::vector<Uptane::Target> installed_versions;
    size_t pending_index = SIZE_MAX;
    storage->loadInstalledVersions(ecu_serial.ToString(), &installed_versions, nullptr, &pending_index);

    if (pending_index < installed_versions.size()) {
      const Uptane::Target &target = installed_versions[pending_index];
      const std::string correlation_id = target.correlation_id();

      data::InstallationResult install_res = package_manager_->finalizeInstall(target);
      storage->saveEcuInstallationResult(ecu_serial, install_res);
      if (install_res.success) {
        storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kCurrent);
        report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(ecu_serial, correlation_id, true));
      } else {
        // finalize failed
        // unset pending flag so that the rest of the uptane process can
        // go forward again
        storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kNone);
        report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(ecu_serial, correlation_id, false));
      }

      computeDeviceInstallationResult(nullptr, correlation_id);
      putManifestSimple();
    } else {
      // nothing found on primary
      LOG_ERROR << "Expected reboot after update on primary but no update found";
    }
  } else {
    LOG_ERROR << "Invalid UPTANE metadata in storage.";
  }

  bootloader->rebootFlagClear();
}

data::InstallationResult SotaUptaneClient::PackageInstallSetResult(const Uptane::Target &target) {
  data::InstallationResult result;
  Uptane::EcuSerial ecu_serial = uptane_manifest.getPrimaryEcuSerial();
  if (!target.IsOstree() && config.pacman.type == PackageManager::kOstree) {
    result = data::InstallationResult(data::ResultCode::Numeric::kValidationFailed,
                                      "Cannot install a non-OSTree package on an OSTree system");
  } else {
    result = PackageInstall(target);
    if (result.result_code.num_code == data::ResultCode::Numeric::kOk) {
      // simple case: update already completed
      storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kCurrent);
    } else if (result.result_code.num_code == data::ResultCode::Numeric::kNeedCompletion) {
      // ostree case: need reboot
      storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kPending);
    }
  }
  storage->saveEcuInstallationResult(ecu_serial, result);
  return result;
}

void SotaUptaneClient::reportHwInfo() {
  Json::Value hw_info = Utils::getHardwareInfo();
  if (!hw_info.empty()) {
    http->put(config.tls.server + "/core/system_info", hw_info);
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

Json::Value SotaUptaneClient::AssembleManifest() {
  Json::Value manifest;  // signed top-level
  Uptane::EcuSerial primary_ecu_serial = uptane_manifest.getPrimaryEcuSerial();

  manifest["primary_ecu_serial"] = primary_ecu_serial.ToString();

  // first part: report current version/state of all ecus
  Json::Value version_manifest;

  Json::Value primary_ecu_version = package_manager_->getManifest(primary_ecu_serial);
  version_manifest[primary_ecu_serial.ToString()] = uptane_manifest.signManifest(primary_ecu_version);

  for (auto it = secondaries.begin(); it != secondaries.end(); it++) {
    Json::Value secmanifest = it->second->getManifest();
    if (secmanifest.isMember("signatures") && secmanifest.isMember("signed")) {
      const auto public_key = it->second->getPublicKey();
      const std::string canonical = Json::FastWriter().write(secmanifest["signed"]);
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

bool SotaUptaneClient::hasPendingUpdates() { return storage->hasPendingInstall(); }

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

bool SotaUptaneClient::updateMeta() {
  // Uptane step 1 (build the vehicle version manifest):
  if (!putManifestSimple()) {
    LOG_ERROR << "could not put manifest";
    return false;
  }
  return uptaneIteration();
}

bool SotaUptaneClient::updateDirectorMeta() {
  // Uptane step 2 (download time) is not implemented yet.
  // Uptane step 3 (download metadata)

  // reset director repo to initial state before starting UPTANE iteration
  director_repo.resetMeta();
  // Load Initial Director Root Metadata
  {
    std::string director_root;
    if (storage->loadLatestRoot(&director_root, Uptane::RepositoryType::Director())) {
      if (!director_repo.initRoot(director_root)) {
        last_exception = director_repo.getLastException();
        return false;
      }
    } else {
      if (!uptane_fetcher->fetchRole(&director_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Director(),
                                     Uptane::Role::Root(), Uptane::Version(1))) {
        return false;
      }
      if (!director_repo.initRoot(director_root)) {
        last_exception = director_repo.getLastException();
        return false;
      }
      storage->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));
    }
  }

  // Update Director Root Metadata
  {
    std::string director_root;
    if (!uptane_fetcher->fetchLatestRole(&director_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Director(),
                                         Uptane::Role::Root())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(director_root);
    int local_version = director_repo.rootVersion();

    for (int version = local_version + 1; version <= remote_version; ++version) {
      if (!uptane_fetcher->fetchRole(&director_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Director(),
                                     Uptane::Role::Root(), Uptane::Version(version))) {
        return false;
      }

      if (!director_repo.verifyRoot(director_root)) {
        last_exception = director_repo.getLastException();
        return false;
      }
      storage->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(version));
      storage->clearNonRootMeta(Uptane::RepositoryType::Director());
    }

    if (director_repo.rootExpired()) {
      last_exception = Uptane::ExpiredMetadata("director", "root");
      return false;
    }
  }
  // Update Director Targets Metadata
  {
    std::string director_targets;

    if (!uptane_fetcher->fetchLatestRole(&director_targets, Uptane::kMaxDirectorTargetsSize,
                                         Uptane::RepositoryType::Director(), Uptane::Role::Targets())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(director_targets);

    int local_version;
    std::string director_targets_stored;
    if (storage->loadNonRoot(&director_targets_stored, Uptane::RepositoryType::Director(), Uptane::Role::Targets())) {
      local_version = Uptane::extractVersionUntrusted(director_targets_stored);
    } else {
      local_version = -1;
    }

    if (!director_repo.verifyTargets(director_targets)) {
      last_exception = director_repo.getLastException();
      return false;
    }

    if (local_version > remote_version) {
      return false;
    } else if (local_version < remote_version) {
      storage->storeNonRoot(director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
    }

    if (director_repo.targetsExpired()) {
      last_exception = Uptane::ExpiredMetadata("director", "targets");
      return false;
    }
  }

  return true;
}

bool SotaUptaneClient::updateImagesMeta() {
  images_repo.resetMeta();
  // Load Initial Images Root Metadata
  {
    std::string images_root;
    if (storage->loadLatestRoot(&images_root, Uptane::RepositoryType::Image())) {
      if (!images_repo.initRoot(images_root)) {
        last_exception = images_repo.getLastException();
        return false;
      }
    } else {
      if (!uptane_fetcher->fetchRole(&images_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Image(),
                                     Uptane::Role::Root(), Uptane::Version(1))) {
        return false;
      }
      if (!images_repo.initRoot(images_root)) {
        last_exception = images_repo.getLastException();
        return false;
      }
      storage->storeRoot(images_root, Uptane::RepositoryType::Image(), Uptane::Version(1));
    }
  }

  // Update Image Root Metadata
  {
    std::string images_root;
    if (!uptane_fetcher->fetchLatestRole(&images_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Image(),
                                         Uptane::Role::Root())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(images_root);
    int local_version = images_repo.rootVersion();

    for (int version = local_version + 1; version <= remote_version; ++version) {
      if (!uptane_fetcher->fetchRole(&images_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Image(),
                                     Uptane::Role::Root(), Uptane::Version(version))) {
        return false;
      }
      if (!images_repo.verifyRoot(images_root)) {
        last_exception = images_repo.getLastException();
        return false;
      }
      storage->storeRoot(images_root, Uptane::RepositoryType::Image(), Uptane::Version(version));
      storage->clearNonRootMeta(Uptane::RepositoryType::Image());
    }

    if (images_repo.rootExpired()) {
      last_exception = Uptane::ExpiredMetadata("repo", "root");
      return false;
    }
  }

  // Update Images Timestamp Metadata
  {
    std::string images_timestamp;

    if (!uptane_fetcher->fetchLatestRole(&images_timestamp, Uptane::kMaxTimestampSize, Uptane::RepositoryType::Image(),
                                         Uptane::Role::Timestamp())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(images_timestamp);

    int local_version;
    std::string images_timestamp_stored;
    if (storage->loadNonRoot(&images_timestamp_stored, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp())) {
      local_version = Uptane::extractVersionUntrusted(images_timestamp_stored);
    } else {
      local_version = -1;
    }

    if (!images_repo.verifyTimestamp(images_timestamp)) {
      last_exception = images_repo.getLastException();
      return false;
    }

    if (local_version > remote_version) {
      return false;
    } else if (local_version < remote_version) {
      storage->storeNonRoot(images_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());
    }

    if (images_repo.timestampExpired()) {
      last_exception = Uptane::ExpiredMetadata("repo", "timestamp");
      return false;
    }
  }

  // Update Images Snapshot Metadata
  {
    std::string images_snapshot;

    int64_t snapshot_size = (images_repo.snapshotSize() > 0) ? images_repo.snapshotSize() : Uptane::kMaxSnapshotSize;
    if (!uptane_fetcher->fetchLatestRole(&images_snapshot, snapshot_size, Uptane::RepositoryType::Image(),
                                         Uptane::Role::Snapshot())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(images_snapshot);

    int local_version;
    std::string images_snapshot_stored;
    if (storage->loadNonRoot(&images_snapshot_stored, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot())) {
      local_version = Uptane::extractVersionUntrusted(images_snapshot_stored);
    } else {
      local_version = -1;
    }

    if (!images_repo.verifySnapshot(images_snapshot)) {
      last_exception = images_repo.getLastException();
      return false;
    }

    if (local_version > remote_version) {
      return false;
    } else if (local_version < remote_version) {
      storage->storeNonRoot(images_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());
    }

    if (images_repo.snapshotExpired()) {
      last_exception = Uptane::ExpiredMetadata("repo", "snapshot");
      return false;
    }
  }

  // Update Images Targets Metadata
  {
    std::string images_targets;
    Uptane::Role targets_role = Uptane::Role::Targets();

    auto targets_size = images_repo.getRoleSize(Uptane::Role::Targets());
    if (targets_size <= 0) {
      targets_size = Uptane::kMaxImagesTargetsSize;
    }
    if (!uptane_fetcher->fetchLatestRole(&images_targets, targets_size, Uptane::RepositoryType::Image(),
                                         targets_role)) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(images_targets);

    int local_version;
    std::string images_targets_stored;
    if (storage->loadNonRoot(&images_targets_stored, Uptane::RepositoryType::Image(), targets_role)) {
      local_version = Uptane::extractVersionUntrusted(images_targets_stored);
    } else {
      local_version = -1;
    }

    if (!images_repo.verifyTargets(images_targets)) {
      last_exception = images_repo.getLastException();
      return false;
    }

    if (local_version > remote_version) {
      return false;
    } else if (local_version < remote_version) {
      storage->storeNonRoot(images_targets, Uptane::RepositoryType::Image(), targets_role);
    }

    if (images_repo.targetsExpired()) {
      last_exception = Uptane::ExpiredMetadata("repo", "targets");
      return false;
    }
  }

  return true;
}

bool SotaUptaneClient::checkDirectorMetaOffline() {
  director_repo.resetMeta();
  // Load Director Root Metadata
  {
    std::string director_root;
    if (!storage->loadLatestRoot(&director_root, Uptane::RepositoryType::Director())) {
      return false;
    }

    if (!director_repo.initRoot(director_root)) {
      last_exception = director_repo.getLastException();
      return false;
    }

    if (director_repo.rootExpired()) {
      last_exception = Uptane::ExpiredMetadata("director", "root");
      return false;
    }
  }

  // Load Director Targets Metadata
  {
    std::string director_targets;

    if (!storage->loadNonRoot(&director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets())) {
      return false;
    }

    if (!director_repo.verifyTargets(director_targets)) {
      last_exception = director_repo.getLastException();
      return false;
    }

    if (director_repo.targetsExpired()) {
      last_exception = Uptane::ExpiredMetadata("director", "targets");
      return false;
    }
  }

  return true;
}

bool SotaUptaneClient::checkImagesMetaOffline() {
  images_repo.resetMeta();
  // Load Images Root Metadata
  {
    std::string images_root;
    if (!storage->loadLatestRoot(&images_root, Uptane::RepositoryType::Image())) {
      return false;
    }

    if (!images_repo.initRoot(images_root)) {
      last_exception = images_repo.getLastException();
      return false;
    }

    if (images_repo.rootExpired()) {
      last_exception = Uptane::ExpiredMetadata("repo", "root");
      return false;
    }
  }

  // Load Images Timestamp Metadata
  {
    std::string images_timestamp;
    if (!storage->loadNonRoot(&images_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp())) {
      return false;
    }

    if (!images_repo.verifyTimestamp(images_timestamp)) {
      last_exception = images_repo.getLastException();
      return false;
    }

    if (images_repo.timestampExpired()) {
      last_exception = Uptane::ExpiredMetadata("repo", "timestamp");
      return false;
    }
  }

  // Load Images Snapshot Metadata
  {
    std::string images_snapshot;

    if (!storage->loadNonRoot(&images_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot())) {
      return false;
    }

    if (!images_repo.verifySnapshot(images_snapshot)) {
      last_exception = images_repo.getLastException();
      return false;
    }

    if (images_repo.snapshotExpired()) {
      last_exception = Uptane::ExpiredMetadata("repo", "snapshot");
      return false;
    }
  }

  // Load Images Targets Metadata
  {
    std::string images_targets;
    Uptane::Role targets_role = Uptane::Role::Targets();
    if (!storage->loadNonRoot(&images_targets, Uptane::RepositoryType::Image(), targets_role)) {
      return false;
    }

    if (!images_repo.verifyTargets(images_targets)) {
      last_exception = images_repo.getLastException();
      return false;
    }

    if (images_repo.targetsExpired()) {
      last_exception = Uptane::ExpiredMetadata("repo", "targets");
      return false;
    }
  }

  return true;
}

void SotaUptaneClient::computeDeviceInstallationResult(data::InstallationResult *result,
                                                       const std::string &correlation_id) {
  data::InstallationResult dev_result;

  std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_results;
  if (!storage->loadEcuInstallationResults(&ecu_results)) {
    dev_result.success = false;
    dev_result.result_code = data::ResultCode::Numeric::kInternalError;
    dev_result.description = "Unable to get installation results from ecus";
  } else {
    for (const auto &r : ecu_results) {
      if (!r.second.success) {
        dev_result.success = false;
        dev_result.result_code = data::ResultCode::Numeric::kInstallFailed;
        dev_result.description = "Installation failed on at least one ecu";
      }
    }
  }

  if (result != nullptr) {
    *result = dev_result;
  }

  storage->storeDeviceInstallationResult(dev_result, "", correlation_id);
}

bool SotaUptaneClient::getNewTargets(std::vector<Uptane::Target> *new_targets, unsigned int *ecus_count) {
  std::vector<Uptane::Target> targets = director_repo.getTargets();
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

      std::vector<Uptane::Target> installed_versions;
      size_t current_version = SIZE_MAX;
      if (!storage->loadInstalledVersions(ecu_serial.ToString(), &installed_versions, &current_version, nullptr)) {
        LOG_WARNING << "Could not load currently installed version for ECU ID: " << ecu_serial.ToString();
        break;
      }

      if (current_version > installed_versions.size()) {
        LOG_WARNING << "Current version for ECU ID: " << ecu_serial.ToString() << " is unknown";
        is_new = true;
      } else if (installed_versions[current_version].filename() != target.filename()) {
        is_new = true;
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

Uptane::Targets SotaUptaneClient::getTrustedDelegation(const Uptane::Role &delegate_role,
                                                       const Uptane::Targets &parent_targets,
                                                       const Uptane::ImagesRepository &images_repo, INvStorage &storage,
                                                       Uptane::Fetcher &fetcher) {
  std::string delegation_meta;

  auto version_in_snapshot = images_repo.getRoleVersion(delegate_role);

  if (storage.loadDelegation(&delegation_meta, delegate_role)) {
    auto version = Uptane::extractVersionUntrusted(delegation_meta);

    if (version > version_in_snapshot) {
      throw Uptane::SecurityException("images", "Rollback attempt on delegated targets");
    } else if (version < version_in_snapshot) {
      delegation_meta.clear();
      storage.deleteDelegation(delegate_role);
    }
  }

  bool delegation_remote = delegation_meta.empty();
  if (delegation_remote) {
    if (!fetcher.fetchLatestRole(&delegation_meta, Uptane::kMaxImagesTargetsSize, Uptane::RepositoryType::Image(),
                                 delegate_role)) {
      throw std::runtime_error(std::string("Couldn't fetch ") +
                               delegate_role.ToString());  // TODO: connectivity exception
    }
  }

  if (!images_repo.verifyRoleHashes(delegation_meta, delegate_role)) {
    throw Uptane::SecurityException("images", "Delegation hash verification failed");
  }

  auto delegation = Uptane::ImagesRepository::verifyDelegation(delegation_meta, delegate_role, parent_targets);

  if (delegation == nullptr) {
    throw Uptane::SecurityException("images", "Delegation verification failed");
  }

  if (delegation_remote) {
    if (delegation->version() != version_in_snapshot) {
      throw Uptane::SecurityException(
          "images", std::string("Delegated role version didn't match the entry in Snapshot metadata"));
    }
    storage.storeDelegation(delegation_meta, delegate_role);
  }

  return *delegation;
}

std::unique_ptr<Uptane::Target> SotaUptaneClient::findTargetHelper(const Uptane::Targets &cur_targets,
                                                                   const Uptane::Target &queried_target, int level,
                                                                   bool terminating) {
  auto it = std::find(cur_targets.targets.cbegin(), cur_targets.targets.cend(), queried_target);
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

    auto delegation = getTrustedDelegation(delegate_role, cur_targets, images_repo, *storage, *uptane_fetcher);

    if (delegation.isExpired(TimeStamp::Now())) {
      continue;
    }

    auto is_terminating = cur_targets.terminating_role_.find(delegate_role);

    if (is_terminating == cur_targets.terminating_role_.end()) {
      throw Uptane::Exception("images", "Inconsistent delegations");
    }

    auto found_target = findTargetHelper(delegation, queried_target, level + 1, is_terminating->second);

    if (found_target != nullptr) {
      return found_target;
    }
  }

  return std::unique_ptr<Uptane::Target>(nullptr);
}

std::unique_ptr<Uptane::Target> SotaUptaneClient::findTargetInDelegationTree(const Uptane::Target &target) {
  auto toplevel_targets = images_repo.getTargets();

  if (toplevel_targets == nullptr) {
    return std::unique_ptr<Uptane::Target>(nullptr);
  }

  return findTargetHelper(*toplevel_targets, target, 0, false);
}

result::Download SotaUptaneClient::downloadImages(const std::vector<Uptane::Target> &targets,
                                                  const api::FlowControlToken *token) {
  // Uptane step 4 - download all the images and verify them against the metadata (for OSTree - pull without
  // deploying)
  std::lock_guard<std::mutex> guard(download_mutex);
  result::Download result;
  std::vector<Uptane::Target> downloaded_targets;
  for (auto it = targets.cbegin(); it != targets.cend(); ++it) {
    auto images_target = findTargetInDelegationTree(*it);
    if (images_target == nullptr) {
      last_exception = Uptane::TargetHashMismatch(it->filename());
      LOG_ERROR << "No matching target in images targets metadata for " << *it;
      result = result::Download(downloaded_targets, result::DownloadStatus::kError, "Target hash mismatch.");
      sendEvent<event::AllDownloadsComplete>(result);
      return result;
    }
  }
  for (auto it = targets.cbegin(); it != targets.cend(); ++it) {
    auto res = downloadImage(*it, token);
    if (res.first) {
      downloaded_targets.push_back(res.second);
    }
  }

  if (!targets.empty()) {
    if (targets.size() == downloaded_targets.size()) {
      result = result::Download(downloaded_targets, result::DownloadStatus::kSuccess, "");
    } else {
      LOG_ERROR << "Only " << downloaded_targets.size() << " of " << targets.size() << " were successfully downloaded.";
      result = result::Download(downloaded_targets, result::DownloadStatus::kPartialSuccess, "");
    }

  } else {
    LOG_INFO << "No new updates to download.";
    result = result::Download({}, result::DownloadStatus::kNothingToDownload, "");
  }
  sendEvent<event::AllDownloadsComplete>(result);
  return result;
}

void SotaUptaneClient::pauseFetching() {
  const std::string &correlation_id = director_repo.getCorrelationId();
  report_queue->enqueue(std_::make_unique<DevicePausedReport>(correlation_id));
}

void SotaUptaneClient::resumeFetching() {
  const std::string &correlation_id = director_repo.getCorrelationId();
  report_queue->enqueue(std_::make_unique<DeviceResumedReport>(correlation_id));
}

std::pair<bool, Uptane::Target> SotaUptaneClient::downloadImage(Uptane::Target target,
                                                                const api::FlowControlToken *token) {
  // TODO: support downloading encrypted targets from director
  // TODO: check if the file is already there before downloading

  const std::string &correlation_id = director_repo.getCorrelationId();
  // send an event for all ecus that are touched by this target
  for (const auto &ecu : target.ecus()) {
    report_queue->enqueue(std_::make_unique<EcuDownloadStartedReport>(ecu.first, correlation_id));
  }

  bool success = false;
  int tries = 3;
  std::chrono::milliseconds wait(500);
  KeyManager keys(storage, config.keymanagerConfig());
  keys.loadKeys();
  auto prog_cb = [this](const Uptane::Target &t, const std::string description, unsigned int progress) {
    report_progress_cb(events_channel.get(), t, description, progress);
  };
  while ((tries--) != 0) {
    success = package_manager_->fetchTarget(target, config.uptane.repo_server, keys, prog_cb, token);
    if (success) {
      break;
    } else if (tries != 0) {
      std::this_thread::sleep_for(wait);
      wait *= 2;
    }
  }
  if (!success) {
    LOG_ERROR << "Download unsuccessful after " << tries << " attempts.";
  }

  // send this asynchronously before `sendEvent`, so that the report timestamp
  // would not be delayed by callbacks on events
  for (const auto &ecu : target.ecus()) {
    report_queue->enqueue(std_::make_unique<EcuDownloadCompletedReport>(ecu.first, correlation_id, success));
  }

  sendEvent<event::DownloadTargetComplete>(target, success);
  return {success, target};
}

bool SotaUptaneClient::uptaneIteration() {
  if (!updateDirectorMeta()) {
    LOG_ERROR << "Failed to update director metadata: " << last_exception.what();
    return false;
  }
  std::vector<Uptane::Target> targets;
  if (!getNewTargets(&targets)) {
    LOG_ERROR << "Inconsistency between director metadata and existent ECUs";
    return false;
  }

  if (targets.empty()) {
    return true;
  }

  LOG_INFO << "got new updates";

  if (!updateImagesMeta()) {
    LOG_ERROR << "Failed to update images metadata: " << last_exception.what();
    return false;
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
    return true;
  }

  LOG_INFO << "got new updates";

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
  putManifestSimple();
  sendEvent<event::SendDeviceDataComplete>();
}

result::UpdateCheck SotaUptaneClient::fetchMeta() {
  result::UpdateCheck result;

  reportNetworkInfo();

  if (hasPendingUpdates()) {
    // no need in update checking if there are some pending updates
    return result::UpdateCheck({}, 0, result::UpdateStatus::kError, Json::nullValue,
                               "There are pending updates, no new updates are checked");
  }

  if (updateMeta()) {
    result = checkUpdates();
  } else {
    result = result::UpdateCheck({}, 0, result::UpdateStatus::kError, Json::nullValue, "Could not update metadata.");
  }
  sendEvent<event::UpdateCheckComplete>(result);

  return result;
}

result::UpdateCheck SotaUptaneClient::checkUpdates() {
  result::UpdateCheck result;

  if (hasPendingUpdates()) {
    // mask updates when an install is pending
    LOG_DEBUG << "An update is pending. Skipping check for update until installation is complete";
    return result;
  }

  std::vector<Uptane::Target> updates;
  unsigned int ecus_count = 0;
  if (!uptaneOfflineIteration(&updates, &ecus_count)) {
    LOG_ERROR << "Invalid UPTANE metadata in storage.";
    result = result::UpdateCheck({}, 0, result::UpdateStatus::kError, Json::nullValue,
                                 "Invalid UPTANE metadata in storage.");
  } else {
    std::string director_targets;
    storage->loadNonRoot(&director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());

    if (!updates.empty()) {
      result = result::UpdateCheck(updates, ecus_count, result::UpdateStatus::kUpdatesAvailable,
                                   Utils::parseJSON(director_targets), "");
      if (updates.size() == 1) {
        LOG_INFO << "1 new update found in Uptane metadata.";
      } else {
        LOG_INFO << updates.size() << " new updates found in Uptane metadata.";
      }
    } else {
      result = result::UpdateCheck(updates, ecus_count, result::UpdateStatus::kNoUpdatesAvailable,
                                   Utils::parseJSON(director_targets), "");
      LOG_DEBUG << "No new updates found in Uptane metadata.";
    }
  }
  return result;
}

result::Install SotaUptaneClient::uptaneInstall(const std::vector<Uptane::Target> &updates) {
  result::Install result;
  Uptane::EcuSerial primary_ecu_serial = uptane_manifest.getPrimaryEcuSerial();

  // clear all old results first
  storage->clearInstallationResults();

  // Uptane step 5 (send time to all ECUs) is not implemented yet.
  std::vector<Uptane::Target> primary_updates = findForEcu(updates, primary_ecu_serial);
  //   6 - send metadata to all the ECUs
  sendMetadataToEcus(updates);

  const std::string &correlation_id = director_repo.getCorrelationId();
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
      bootloader->updateNotify();
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

bool SotaUptaneClient::isInstallCompletionRequired() {
  bool force_install_completion = (hasPendingUpdates() && config.uptane.force_install_completion);
  return force_install_completion;
}

void SotaUptaneClient::completeInstall() {
  if (isInstallCompletionRequired()) {
    package_manager_->completeInstall();
  }
}

bool SotaUptaneClient::putManifestSimple() {
  // does not send event, so it can be used as a subset of other steps
  auto manifest = AssembleManifest();
  if (hasPendingUpdates()) {
    LOG_DEBUG << "An update is pending. Skipping manifest upload until installation is complete";
    return false;
  }

  auto signed_manifest = uptane_manifest.signManifest(manifest);
  HttpResponse response = http->put(config.uptane.director_server + "/manifest", signed_manifest);
  if (response.isOk()) {
    storage->clearInstallationResults();
    return true;
  }
  return false;
}

bool SotaUptaneClient::putManifest() {
  bool success = putManifestSimple();
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
  store_it = std::find_if(serials.begin(), serials.end(), primary_comp);
  if (store_it == serials.end()) {
    LOG_ERROR << "Primary ECU serial " << uptane_manifest.getPrimaryEcuSerial() << " not found in storage!";
    misconfigured_ecus.emplace_back(uptane_manifest.getPrimaryEcuSerial(), Uptane::HardwareIdentifier(""),
                                    EcuState::kOld);
  } else {
    found[static_cast<size_t>(std::distance(serials.cbegin(), store_it))] = true;
  }

  std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface>>::const_iterator it;
  for (it = secondaries.cbegin(); it != secondaries.cend(); ++it) {
    SerialCompare secondary_comp(it->second->getSerial());
    store_it = std::find_if(serials.begin(), serials.end(), secondary_comp);
    if (store_it == serials.end()) {
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

std::future<bool> SotaUptaneClient::sendFirmwareAsync(Uptane::SecondaryInterface &secondary,
                                                      const std::shared_ptr<std::string> &data) {
  auto f = [this, &secondary, data]() {
    const std::string &correlation_id = director_repo.getCorrelationId();
    sendEvent<event::InstallStarted>(secondary.getSerial());
    report_queue->enqueue(std_::make_unique<EcuInstallationStartedReport>(secondary.getSerial(), correlation_id));
    bool ret = secondary.sendFirmware(data);
    report_queue->enqueue(
        std_::make_unique<EcuInstallationCompletedReport>(secondary.getSerial(), correlation_id, ret));
    sendEvent<event::InstallTargetComplete>(secondary.getSerial(), ret);

    return ret;
  };

  return std::async(std::launch::async, f);
}

std::vector<result::Install::EcuReport> SotaUptaneClient::sendImagesToEcus(const std::vector<Uptane::Target> &targets) {
  std::vector<result::Install::EcuReport> reports;
  std::vector<std::pair<result::Install::EcuReport, std::future<bool>>> firmwareFutures;

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

      if (sec.sconfig.secondary_type == Uptane::SecondaryType::kOpcuaUptane) {
        Json::Value data;
        data["sysroot_path"] = config.pacman.sysroot.string();
        data["ref_hash"] = targets_it->sha256Hash();
        firmwareFutures.emplace_back(result::Install::EcuReport(*targets_it, ecu_serial, data::InstallationResult()),
                                     sendFirmwareAsync(sec, std::make_shared<std::string>(Utils::jsonToStr(data))));
      } else if (targets_it->IsOstree()) {
        // empty firmware means OSTree secondaries: pack credentials instead
        const std::string creds_archive = secondaryTreehubCredentials();
        if (creds_archive.empty()) {
          continue;
        }
        firmwareFutures.emplace_back(result::Install::EcuReport(*targets_it, ecu_serial, data::InstallationResult()),
                                     sendFirmwareAsync(sec, std::make_shared<std::string>(creds_archive)));
      } else {
        std::stringstream sstr;
        sstr << *storage->openTargetFile(*targets_it);
        const std::string fw = sstr.str();
        firmwareFutures.emplace_back(result::Install::EcuReport(*targets_it, ecu_serial, data::InstallationResult()),
                                     sendFirmwareAsync(sec, std::make_shared<std::string>(fw)));
      }
    }
  }

  for (auto &f : firmwareFutures) {
    bool fut_result = f.second.get();
    if (fut_result) {
      f.first.install_res = data::InstallationResult(data::ResultCode::Numeric::kOk, "");
      storage->saveInstalledVersion(f.first.serial.ToString(), f.first.update, InstalledVersionUpdateMode::kCurrent);
    } else {
      f.first.install_res = data::InstallationResult(data::ResultCode::Numeric::kInstallFailed, "");
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

LazyTargetsList SotaUptaneClient::allTargets() { return LazyTargetsList(images_repo, storage, uptane_fetcher); }

LazyTargetsList::DelegationIterator::DelegationIterator(const Uptane::ImagesRepository &repo,
                                                        std::shared_ptr<INvStorage> storage,
                                                        std::shared_ptr<Uptane::Fetcher> fetcher, bool is_end)
    : repo_{repo}, storage_{std::move(storage)}, fetcher_{std::move(fetcher)}, is_end_{is_end} {
  tree_ = std::make_shared<DelegatedTargetTreeNode>();
  tree_node_ = tree_.get();

  tree_node_->role = Uptane::Role::Targets();
  cur_targets_ = repo_.getTargets();
}

void LazyTargetsList::DelegationIterator::renewTargetsData() {
  auto role = tree_node_->role;

  if (role == Uptane::Role::Targets()) {
    cur_targets_ = repo_.getTargets();
  } else {
    // go to the top of the delegation tree
    std::stack<std::vector<std::shared_ptr<DelegatedTargetTreeNode>>::size_type> indices;
    auto node = tree_node_->parent;
    while (node->parent != nullptr) {
      indices.push(node->parent_idx);
      node = node->parent;
    }

    auto parent_targets = repo_.getTargets();
    while (!indices.empty()) {
      auto idx = indices.top();
      indices.pop();

      auto fetched_role = Uptane::Role(parent_targets->delegated_role_names_[idx], true);
      parent_targets = std::make_shared<const Uptane::Targets>(
          SotaUptaneClient::getTrustedDelegation(fetched_role, *parent_targets, repo_, *storage_, *fetcher_));
    }
    cur_targets_ = std::make_shared<Uptane::Targets>(
        SotaUptaneClient::getTrustedDelegation(role, *parent_targets, repo_, *storage_, *fetcher_));
  }
}

bool LazyTargetsList::DelegationIterator::operator==(const LazyTargetsList::DelegationIterator &other) const {
  if (is_end_ && other.is_end_) {
    return true;
  }

  return is_end_ == other.is_end_ && tree_node_->role == other.tree_node_->role && target_idx_ == other.target_idx_;
}

const Uptane::Target &LazyTargetsList::DelegationIterator::operator*() {
  if (is_end_) {
    throw std::runtime_error("Inconsistent delegation iterator");  // TODO: UptaneException
  }

  if (!cur_targets_) {
    renewTargetsData();
  }

  if (target_idx_ >= cur_targets_->targets.size()) {
    throw std::runtime_error("Inconsistent delegation iterator");  // TODO: UptaneException
  }

  return cur_targets_->targets[target_idx_];
}

LazyTargetsList::DelegationIterator LazyTargetsList::DelegationIterator::operator++() {
  if (is_end_) {
    return *this;
  }

  if (!cur_targets_) {
    renewTargetsData();
  }

  // first iterate over current role's targets
  if (target_idx_ + 1 < cur_targets_->targets.size()) {
    ++target_idx_;
    return *this;
  }

  // then go to children delegations
  if (terminating_ || level_ >= Uptane::kDelegationsMaxDepth) {
    // but only if this delegation is not terminating
    is_end_ = true;
    return *this;
  }

  // populate the next level of the delegation tree if it's not populated yet
  if (cur_targets_->delegated_role_names_.size() != tree_node_->children.size()) {
    tree_node_->children.clear();

    for (std::vector<std::shared_ptr<DelegatedTargetTreeNode>>::size_type i = 0;
         i < cur_targets_->delegated_role_names_.size(); ++i) {
      auto new_node = std::make_shared<DelegatedTargetTreeNode>();

      new_node->role = Uptane::Role(cur_targets_->delegated_role_names_[i], true);
      new_node->parent = tree_node_;
      new_node->parent_idx = i;

      tree_node_->children.push_back(new_node);
    }
  }

  if (children_idx_ < tree_node_->children.size()) {
    auto new_tree_node = tree_node_->children[children_idx_].get();
    target_idx_ = 0;
    children_idx_ = 0;
    ++level_;

    auto terminating_it = cur_targets_->terminating_role_.find(new_tree_node->role);
    if (terminating_it == cur_targets_->terminating_role_.end()) {
      throw std::runtime_error("Inconsistent delegation tree");
    }
    terminating_ = terminating_it->second;

    cur_targets_.reset();
    tree_node_ = new_tree_node;
    return *this;
  }

  // then go to the parent delegation
  if (tree_node_->parent != nullptr) {
    auto new_tree_node = tree_node_->parent;
    children_idx_ = tree_node_->parent_idx + 1;
    --level_;
    terminating_ = false;

    cur_targets_.reset();
    tree_node_ = new_tree_node;
    renewTargetsData();
    target_idx_ = cur_targets_->targets.size();  // mark targets as exhausted
    return ++(*this);                            // reiterate to find the next target
  }

  // then give up
  is_end_ = true;
  return *this;
}
