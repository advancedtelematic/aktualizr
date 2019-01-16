#include "sotauptaneclient.h"

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
  package_manager_ = PackageManagerFactory::makePackageManager(config.pacman, storage, bootloader);
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

data::InstallOutcome SotaUptaneClient::PackageInstall(const Uptane::Target &target) {
  LOG_INFO << "Installing package using " << package_manager_->name() << " package manager";
  try {
    return package_manager_->install(target);
  } catch (std::exception &ex) {
    return data::InstallOutcome(data::UpdateResultCode::kInstallFailed, ex.what());
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

      data::InstallOutcome outcome = package_manager_->finalizeInstall(target);
      if (outcome.first == data::UpdateResultCode::kOk) {
        storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kCurrent);
        report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(ecu_serial, correlation_id, true));
      } else {
        // finalize failed
        // unset pending flag so that the rest of the uptane process can
        // go forward again
        storage->saveInstalledVersion(ecu_serial.ToString(), target, InstalledVersionUpdateMode::kNone);
        report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(ecu_serial, correlation_id, false));
      }

      // TODO: this should be per-ecu like installed versions!
      storage->storeInstallationResult(data::OperationResult::fromOutcome(target.filename(), outcome));
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

data::OperationResult SotaUptaneClient::PackageInstallSetResult(const Uptane::Target &target) {
  data::OperationResult result;
  if (!target.IsOstree() && config.pacman.type == PackageManager::kOstree) {
    data::InstallOutcome outcome(data::UpdateResultCode::kValidationFailed,
                                 "Cannot install a non-OSTree package on an OSTree system");
    result = data::OperationResult::fromOutcome(target.filename(), outcome);
  } else {
    result = data::OperationResult::fromOutcome(target.filename(), PackageInstall(target));
    if (result.result_code == data::UpdateResultCode::kOk) {
      // simple case: update already completed
      storage->saveInstalledVersion(uptane_manifest.getPrimaryEcuSerial().ToString(), target,
                                    InstalledVersionUpdateMode::kCurrent);
    } else if (result.result_code == data::UpdateResultCode::kNeedCompletion) {
      // ostree case: need reboot
      storage->saveInstalledVersion(uptane_manifest.getPrimaryEcuSerial().ToString(), target,
                                    InstalledVersionUpdateMode::kPending);
    }
  }
  storage->storeInstallationResult(result);
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
  Json::Value result;
  Json::Value unsigned_ecu_version = package_manager_->getManifest(uptane_manifest.getPrimaryEcuSerial());

  data::OperationResult installation_result;
  storage->loadInstallationResult(&installation_result);
  if (!installation_result.id.empty()) {
    unsigned_ecu_version["custom"]["operation_result"] = installation_result.toJson();
  }

  result[uptane_manifest.getPrimaryEcuSerial().ToString()] = uptane_manifest.signVersionManifest(unsigned_ecu_version);
  for (auto it = secondaries.begin(); it != secondaries.end(); it++) {
    Json::Value secmanifest = it->second->getManifest();
    if (secmanifest.isMember("signatures") && secmanifest.isMember("signed")) {
      const auto public_key = it->second->getPublicKey();
      const std::string canonical = Json::FastWriter().write(secmanifest["signed"]);
      const bool verified = public_key.VerifySignature(secmanifest["signatures"][0]["sig"].asString(), canonical);
      if (verified) {
        result[it->first.ToString()] = secmanifest;
      } else {
        LOG_ERROR << "Secondary manifest verification failed, manifest: " << secmanifest;
      }
    } else {
      LOG_ERROR << "Secondary manifest is corrupted or not signed, manifest: " << secmanifest;
    }
  }
  return result;
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
  reportNetworkInfo();
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

    int64_t targets_size = (images_repo.targetsSize() > 0) ? images_repo.targetsSize() : Uptane::kMaxImagesTargetsSize;
    if (!uptane_fetcher->fetchLatestRole(&images_targets, targets_size, Uptane::RepositoryType::Image(),
                                         Uptane::Role::Targets())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(images_targets);

    int local_version;
    std::string images_targets_stored;
    if (storage->loadNonRoot(&images_targets_stored, Uptane::RepositoryType::Image(), Uptane::Role::Targets())) {
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
      storage->storeNonRoot(images_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets());
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
    if (!storage->loadNonRoot(&images_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets())) {
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
        LOG_WARNING << "Unknown ECU ID in director targets metadata: " << ecu_serial.ToString();
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

result::Download SotaUptaneClient::downloadImages(const std::vector<Uptane::Target> &targets) {
  // Uptane step 4 - download all the images and verify them against the metadata (for OSTree - pull without
  // deploying)
  std::lock_guard<std::mutex> guard(download_mutex);
  result::Download result;
  std::vector<Uptane::Target> downloaded_targets;
  for (auto it = targets.cbegin(); it != targets.cend(); ++it) {
    // TODO: delegations
    auto images_target = images_repo.getTarget(*it);
    if (images_target == nullptr) {
      last_exception = Uptane::TargetHashMismatch(it->filename());
      LOG_ERROR << "No matching target in images targets metadata for " << *it;
      result = result::Download(downloaded_targets, result::DownloadStatus::kError, "Target hash mismatch.");
      sendEvent<event::AllDownloadsComplete>(result);
      return result;
    }
  }
  for (auto it = targets.cbegin(); it != targets.cend(); ++it) {
    auto res = downloadImage(*it);
    if (res.first) {
      downloaded_targets.push_back(res.second);
    }
  }

  if (!targets.empty()) {
    if (targets.size() == downloaded_targets.size()) {
      result = result::Download(downloaded_targets, result::DownloadStatus::kSuccess, "");
    } else {
      LOG_ERROR << "Only " << downloaded_targets.size() << " of " << targets.size()
                << " were successfully downloaded. Report not sent.";
      result = result::Download(downloaded_targets, result::DownloadStatus::kPartialSuccess, "");
    }

  } else {
    LOG_INFO << "No new updates to download.";
    result = result::Download({}, result::DownloadStatus::kNothingToDownload, "");
  }
  sendEvent<event::AllDownloadsComplete>(result);
  return result;
}

result::Pause SotaUptaneClient::pause() {
  result::Pause res = uptane_fetcher->setPause(true);

  sendEvent<event::DownloadPaused>(res);

  return res;
}

result::Pause SotaUptaneClient::resume() {
  result::Pause res = uptane_fetcher->setPause(false);

  sendEvent<event::DownloadResumed>(res);

  return res;
}

std::pair<bool, Uptane::Target> SotaUptaneClient::downloadImage(Uptane::Target target) {
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
  while ((tries--) != 0) {
    success = uptane_fetcher->fetchVerifyTarget(target);
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
    LOG_INFO << "An update is pending, checking for updates is disabled";
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
  // Uptane step 5 (send time to all ECUs) is not implemented yet.
  std::vector<Uptane::Target> primary_updates = findForEcu(updates, uptane_manifest.getPrimaryEcuSerial());
  //   6 - send metadata to all the ECUs
  sendMetadataToEcus(updates);

  const std::string &correlation_id = director_repo.getCorrelationId();
  //   7 - send images to ECUs (deploy for OSTree)
  if (primary_updates.size() != 0u) {
    // assuming one OSTree OS per primary => there can be only one OSTree update
    Uptane::Target primary_update = primary_updates[0];
    primary_update.setCorrelationId(correlation_id);

    report_queue->enqueue(
        std_::make_unique<EcuInstallationStartedReport>(uptane_manifest.getPrimaryEcuSerial(), correlation_id));
    sendEvent<event::InstallStarted>(uptane_manifest.getPrimaryEcuSerial());

    data::OperationResult status;
    if (!isInstalledOnPrimary(primary_update)) {
      // notify the bootloader before installation happens, because installation is not atomic and
      //   a false notification doesn't hurt when rollbacks are implemented
      bootloader->updateNotify();
      status = PackageInstallSetResult(primary_update);
      Uptane::EcuSerial ecu_serial = uptane_manifest.getPrimaryEcuSerial();
      if (status.result_code == data::UpdateResultCode::kNeedCompletion) {
        // update needs a reboot, send distinct EcuInstallationApplied event
        report_queue->enqueue(std_::make_unique<EcuInstallationAppliedReport>(ecu_serial, correlation_id));
        sendEvent<event::InstallTargetComplete>(ecu_serial, true);  // TODO: distinguish from success here?
      } else if (status.result_code == data::UpdateResultCode::kOk) {
        report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(ecu_serial, correlation_id, true));
        sendEvent<event::InstallTargetComplete>(ecu_serial, true);
      } else {
        // general error case
        report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(ecu_serial, correlation_id, false));
        sendEvent<event::InstallTargetComplete>(ecu_serial, false);
      }
    } else {
      data::InstallOutcome outcome(data::UpdateResultCode::kAlreadyProcessed, "Package already installed");
      status = data::OperationResult(primary_update.filename(), outcome);
      storage->storeInstallationResult(status);
      // TODO: distinguish this case from regular failure for local and remote
      // event reporting
      report_queue->enqueue(std_::make_unique<EcuInstallationCompletedReport>(uptane_manifest.getPrimaryEcuSerial(),
                                                                              correlation_id, false));
      sendEvent<event::InstallTargetComplete>(uptane_manifest.getPrimaryEcuSerial(), false);
    }
    result.reports.emplace(result.reports.begin(), primary_update, uptane_manifest.getPrimaryEcuSerial(), status);
    // TODO: other updates for primary
  } else {
    LOG_INFO << "No update to install on primary";
  }

  auto sec_reports = sendImagesToEcus(updates);
  result.reports.insert(result.reports.end(), sec_reports.begin(), sec_reports.end());
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

bool SotaUptaneClient::putManifestSimple() {
  // does not send event, so it can be used as a subset of other steps
  auto manifest = AssembleManifest();
  if (hasPendingUpdates()) {
    LOG_INFO << "An update is pending, putManifest is disabled";
    return false;
  }

  auto signed_manifest = uptane_manifest.signManifest(manifest);
  HttpResponse response = http->put(config.uptane.director_server + "/manifest", signed_manifest);
  if (response.isOk()) {
    storage->clearInstallationResult();
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
    misconfigured_ecus.emplace_back(store_it->first, store_it->second, EcuState::kOld);
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

std::vector<result::InstallReport> SotaUptaneClient::sendImagesToEcus(const std::vector<Uptane::Target> &targets) {
  std::vector<result::InstallReport> reports;
  std::vector<std::pair<result::InstallReport, std::future<bool>>> firmwareFutures;

  // target images should already have been downloaded to metadata_path/targets/
  for (auto targets_it = targets.cbegin(); targets_it != targets.cend(); ++targets_it) {
    for (auto ecus_it = targets_it->ecus().cbegin(); ecus_it != targets_it->ecus().cend(); ++ecus_it) {
      const Uptane::EcuSerial &ecu_serial = ecus_it->first;

      auto f = secondaries.find(ecu_serial);
      if (f == secondaries.end()) {
        continue;
      }
      Uptane::SecondaryInterface &sec = *f->second;

      if (sec.sconfig.secondary_type == Uptane::SecondaryType::kOpcuaUptane) {
        Json::Value data;
        data["sysroot_path"] = config.pacman.sysroot.string();
        data["ref_hash"] = targets_it->sha256Hash();
        firmwareFutures.emplace_back(std::pair<result::InstallReport, std::future<bool>>(
            result::InstallReport(*targets_it, ecu_serial, data::OperationResult()),
            sendFirmwareAsync(sec, std::make_shared<std::string>(Utils::jsonToStr(data)))));
      } else if (targets_it->IsOstree()) {
        // empty firmware means OSTree secondaries: pack credentials instead
        const std::string creds_archive = secondaryTreehubCredentials();
        if (creds_archive.empty()) {
          continue;
        }
        firmwareFutures.emplace_back(std::pair<result::InstallReport, std::future<bool>>(
            result::InstallReport(*targets_it, ecu_serial, data::OperationResult()),
            sendFirmwareAsync(sec, std::make_shared<std::string>(creds_archive))));
      } else {
        std::stringstream sstr;
        sstr << *storage->openTargetFile(*targets_it);
        const std::string fw = sstr.str();
        firmwareFutures.emplace_back(std::pair<result::InstallReport, std::future<bool>>(
            result::InstallReport(*targets_it, ecu_serial, data::OperationResult()),
            sendFirmwareAsync(sec, std::make_shared<std::string>(fw))));
      }
    }
  }

  for (auto &f : firmwareFutures) {
    bool fut_result = f.second.get();
    if (fut_result) {
      f.first.status = data::OperationResult(f.first.update.filename(), data::UpdateResultCode::kOk, "");
      storage->saveInstalledVersion(f.first.serial.ToString(), f.first.update, InstalledVersionUpdateMode::kCurrent);
    } else {
      f.first.status = data::OperationResult(f.first.update.filename(), data::UpdateResultCode::kInstallFailed, "");
    }
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

const std::vector<Uptane::Target> SotaUptaneClient::GetRepoTargets() {
  if (!updateImagesMeta()) {
    LOG_ERROR << "Unable to get latest repo data";
  }
  return images_repo.getTargets();
}
