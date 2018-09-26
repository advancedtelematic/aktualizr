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

std::shared_ptr<SotaUptaneClient> SotaUptaneClient::newDefaultClient(
    Config &config_in, std::shared_ptr<INvStorage> storage_in, std::shared_ptr<event::Channel> events_channel_in) {
  std::shared_ptr<HttpClient> http_client_in = std::make_shared<HttpClient>();
  std::shared_ptr<Uptane::Fetcher> uptane_fetcher =
      std::make_shared<Uptane::Fetcher>(config_in, storage_in, http_client_in, events_channel_in);
  std::shared_ptr<Bootloader> bootloader_in = std::make_shared<Bootloader>(config_in.bootloader);
  std::shared_ptr<ReportQueue> report_queue_in = std::make_shared<ReportQueue>(config_in, http_client_in);

  return std::make_shared<SotaUptaneClient>(config_in, storage_in, http_client_in, uptane_fetcher, bootloader_in,
                                            report_queue_in, events_channel_in);
}

std::shared_ptr<SotaUptaneClient> SotaUptaneClient::newTestClient(Config &config_in,
                                                                  std::shared_ptr<INvStorage> storage_in,
                                                                  std::shared_ptr<HttpInterface> http_client_in,
                                                                  std::shared_ptr<event::Channel> events_channel_in) {
  std::shared_ptr<Uptane::Fetcher> uptane_fetcher =
      std::make_shared<Uptane::Fetcher>(config_in, storage_in, http_client_in, events_channel_in);
  std::shared_ptr<Bootloader> bootloader_in = std::make_shared<Bootloader>(config_in.bootloader);
  std::shared_ptr<ReportQueue> report_queue_in = std::make_shared<ReportQueue>(config_in, http_client_in);
  return std::make_shared<SotaUptaneClient>(config_in, storage_in, http_client_in, uptane_fetcher, bootloader_in,
                                            report_queue_in, events_channel_in);
}

SotaUptaneClient::SotaUptaneClient(Config &config_in, std::shared_ptr<INvStorage> storage_in,
                                   std::shared_ptr<HttpInterface> http_client,
                                   std::shared_ptr<Uptane::Fetcher> uptane_fetcher_in,
                                   std::shared_ptr<Bootloader> bootloader_in,
                                   std::shared_ptr<ReportQueue> report_queue_in,
                                   std::shared_ptr<event::Channel> events_channel_in)
    : config(config_in),
      uptane_manifest(config, storage_in),
      storage(std::move(storage_in)),
      http(std::move(http_client)),
      uptane_fetcher(std::move(uptane_fetcher_in)),
      bootloader(std::move(bootloader_in)),
      report_queue(std::move(report_queue_in)),
      events_channel(std::move(events_channel_in)) {
  // consider boot successful as soon as we started, missing internet connection or connection to secondaries are not
  // proper reasons to roll back
  package_manager_ = PackageManagerFactory::makePackageManager(config.pacman, storage);
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
    LOG_WARNING << "Multiple secondaries found with the same serial: " << sec_serial;
    return;
  }
  sec->addEventsChannel(events_channel);
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

void SotaUptaneClient::PackageInstallSetResult(const Uptane::Target &target) {
  data::OperationResult result;
  if (!target.IsOstree() && config.pacman.type == PackageManager::kOstree) {
    data::InstallOutcome outcome(data::UpdateResultCode::kValidationFailed,
                                 "Cannot install a non-OSTree package on an OSTree system");
    result = data::OperationResult::fromOutcome(target.filename(), outcome);
  } else {
    result = data::OperationResult::fromOutcome(target.filename(), PackageInstall(target));
    if (result.result_code == data::UpdateResultCode::kOk) {
      storage->saveInstalledVersion(target);
    }
  }
  storage->storeInstallationResult(result);
}

void SotaUptaneClient::reportHwInfo() {
  Json::Value hw_info = Utils::getHardwareInfo();
  if (!hw_info.empty()) {
    http->put(config.tls.server + "/core/system_info", hw_info);
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
  installed_images.clear();
  Json::Value unsigned_ecu_version = package_manager_->getManifest(uptane_manifest.getPrimaryEcuSerial());

  data::OperationResult installation_result;
  storage->loadInstallationResult(&installation_result);
  if (!installation_result.id.empty()) {
    unsigned_ecu_version["custom"]["operation_result"] = installation_result.toJson();
  }

  installed_images[uptane_manifest.getPrimaryEcuSerial()] =
      unsigned_ecu_version["installed_image"]["filepath"].asString();

  result[uptane_manifest.getPrimaryEcuSerial().ToString()] = uptane_manifest.signVersionManifest(unsigned_ecu_version);
  for (auto it = secondaries.begin(); it != secondaries.end(); it++) {
    Json::Value secmanifest = it->second->getManifest();
    if (secmanifest.isMember("signatures") && secmanifest.isMember("signed")) {
      const auto public_key = it->second->getPublicKey();
      const std::string canonical = Json::FastWriter().write(secmanifest["signed"]);
      const bool verified = public_key.VerifySignature(secmanifest["signatures"][0]["sig"].asString(), canonical);
      if (verified) {
        result[it->first.ToString()] = secmanifest;
        installed_images[it->first] = secmanifest["signed"]["installed_image"]["filepath"].asString();
      } else {
        LOG_ERROR << "Secondary manifest verification failed, manifest: " << secmanifest;
      }
    } else {
      LOG_ERROR << "Secondary manifest is corrupted or not signed, manifest: " << secmanifest;
    }
  }
  return result;
}

bool SotaUptaneClient::hasPendingUpdates(const Json::Value &manifests) {
  for (auto manifest : manifests) {
    if (manifest["signed"].isMember("custom")) {
      auto status =
          static_cast<data::UpdateResultCode>(manifest["signed"]["custom"]["operation_result"]["result_code"].asUInt());
      if (status == data::UpdateResultCode::kInProgress) {
        return true;
      }
    }
  }
  return false;
}

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
    if (storage->loadLatestRoot(&director_root, Uptane::RepositoryType::Director)) {
      if (!director_repo.initRoot(director_root)) {
        last_exception = director_repo.getLastException();
        return false;
      }
    } else {
      if (!uptane_fetcher->fetchRole(&director_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Director,
                                     Uptane::Role::Root(), Uptane::Version(1))) {
        return false;
      }
      if (!director_repo.initRoot(director_root)) {
        last_exception = director_repo.getLastException();
        return false;
      }
      storage->storeRoot(director_root, Uptane::RepositoryType::Director, Uptane::Version(1));
    }
  }

  // Update Director Root Metadata
  {
    std::string director_root;
    if (!uptane_fetcher->fetchLatestRole(&director_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Director,
                                         Uptane::Role::Root())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(director_root);
    int local_version = director_repo.rootVersion();

    for (int version = local_version + 1; version <= remote_version; ++version) {
      if (!uptane_fetcher->fetchRole(&director_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Director,
                                     Uptane::Role::Root(), Uptane::Version(version))) {
        return false;
      }

      if (!director_repo.verifyRoot(director_root)) {
        last_exception = director_repo.getLastException();
        return false;
      }
      storage->storeRoot(director_root, Uptane::RepositoryType::Director, Uptane::Version(version));
      storage->clearNonRootMeta(Uptane::RepositoryType::Director);
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
                                         Uptane::RepositoryType::Director, Uptane::Role::Targets())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(director_targets);

    int local_version;
    std::string director_targets_stored;
    if (storage->loadNonRoot(&director_targets_stored, Uptane::RepositoryType::Director, Uptane::Role::Targets())) {
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
      storage->storeNonRoot(director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets());
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
    if (storage->loadLatestRoot(&images_root, Uptane::RepositoryType::Images)) {
      if (!images_repo.initRoot(images_root)) {
        last_exception = images_repo.getLastException();
        return false;
      }
    } else {
      if (!uptane_fetcher->fetchRole(&images_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Images,
                                     Uptane::Role::Root(), Uptane::Version(1))) {
        return false;
      }
      if (!images_repo.initRoot(images_root)) {
        last_exception = images_repo.getLastException();
        return false;
      }
      storage->storeRoot(images_root, Uptane::RepositoryType::Images, Uptane::Version(1));
    }
  }

  // Update Image Root Metadata
  {
    std::string images_root;
    if (!uptane_fetcher->fetchLatestRole(&images_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Images,
                                         Uptane::Role::Root())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(images_root);
    int local_version = images_repo.rootVersion();

    for (int version = local_version + 1; version <= remote_version; ++version) {
      if (!uptane_fetcher->fetchRole(&images_root, Uptane::kMaxRootSize, Uptane::RepositoryType::Images,
                                     Uptane::Role::Root(), Uptane::Version(version))) {
        return false;
      }
      if (!images_repo.verifyRoot(images_root)) {
        last_exception = images_repo.getLastException();
        return false;
      }
      storage->storeRoot(images_root, Uptane::RepositoryType::Images, Uptane::Version(version));
      storage->clearNonRootMeta(Uptane::RepositoryType::Images);
    }

    if (images_repo.rootExpired()) {
      last_exception = Uptane::ExpiredMetadata("repo", "root");
      return false;
    }
  }

  // Update Images Timestamp Metadata
  {
    std::string images_timestamp;

    if (!uptane_fetcher->fetchLatestRole(&images_timestamp, Uptane::kMaxTimestampSize, Uptane::RepositoryType::Images,
                                         Uptane::Role::Timestamp())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(images_timestamp);

    int local_version;
    std::string images_timestamp_stored;
    if (storage->loadNonRoot(&images_timestamp_stored, Uptane::RepositoryType::Images, Uptane::Role::Timestamp())) {
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
      storage->storeNonRoot(images_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp());
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
    if (!uptane_fetcher->fetchLatestRole(&images_snapshot, snapshot_size, Uptane::RepositoryType::Images,
                                         Uptane::Role::Snapshot())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(images_snapshot);

    int local_version;
    std::string images_snapshot_stored;
    if (storage->loadNonRoot(&images_snapshot_stored, Uptane::RepositoryType::Images, Uptane::Role::Snapshot())) {
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
      storage->storeNonRoot(images_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot());
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
    if (!uptane_fetcher->fetchLatestRole(&images_targets, targets_size, Uptane::RepositoryType::Images,
                                         Uptane::Role::Targets())) {
      return false;
    }
    int remote_version = Uptane::extractVersionUntrusted(images_targets);

    int local_version;
    std::string images_targets_stored;
    if (storage->loadNonRoot(&images_targets_stored, Uptane::RepositoryType::Images, Uptane::Role::Targets())) {
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
      storage->storeNonRoot(images_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets());
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
    if (!storage->loadLatestRoot(&director_root, Uptane::RepositoryType::Director)) {
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

    if (!storage->loadNonRoot(&director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets())) {
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
    if (!storage->loadLatestRoot(&images_root, Uptane::RepositoryType::Images)) {
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
    if (!storage->loadNonRoot(&images_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp())) {
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

    if (!storage->loadNonRoot(&images_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot())) {
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
    if (!storage->loadNonRoot(&images_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets())) {
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
  for (auto targets_it = targets.cbegin(); targets_it != targets.cend(); ++targets_it) {
    bool is_new = false;
    for (auto ecus_it = targets_it->ecus().cbegin(); ecus_it != targets_it->ecus().cend(); ++ecus_it) {
      Uptane::EcuSerial ecu_serial = ecus_it->first;
      Uptane::HardwareIdentifier hw_id = ecus_it->second;

      auto hwid_it = hw_ids.find(ecu_serial);
      if (hwid_it == hw_ids.end()) {
        LOG_WARNING << "Unknown ECU ID in director targets metadata: " << ecu_serial.ToString();
        last_exception = Uptane::BadEcuId(targets_it->filename());
        return false;
      }

      if (hwid_it->second != hw_id) {
        LOG_ERROR << "Wrong hardware identifier for ECU " << ecu_serial.ToString();
        last_exception = Uptane::BadHardwareId(targets_it->filename());
        return false;
      }

      auto images_it = installed_images.find(ecu_serial);
      if (images_it == installed_images.end()) {
        LOG_WARNING << "Unknown ECU ID on the device: " << ecu_serial.ToString();
        break;
      }
      if (images_it->second != targets_it->filename()) {
        is_new = true;
        if (ecus_count != nullptr) {
          (*ecus_count)++;
        }
      }
      // no updates for this image => continue
    }
    if (is_new) {
      new_targets->push_back(*targets_it);
    }
  }
  return true;
}

bool SotaUptaneClient::downloadImages(const std::vector<Uptane::Target> &targets) {
  // Uptane step 4 - download all the images and verify them against the metadata (for OSTree - pull without
  // deploying)
  download_futures_.clear();
  std::vector<Uptane::Target> downloaded_targets;
  for (auto it = targets.cbegin(); it != targets.cend(); ++it) {
    // TODO: delegations
    auto images_target = images_repo.getTarget(*it);
    if (images_target == nullptr) {
      last_exception = Uptane::TargetHashMismatch(it->filename());
      LOG_ERROR << "No matching target in images targets metadata for " << *it;
      sendEvent<event::Error>("Target hash mismatch.");
      return false;
    }
    download_futures_.push_back(std::async(std::launch::async, &SotaUptaneClient::downloadImage, this, *it));
  }
  for (auto &f : download_futures_) {
    auto result = f.get();
    if (result.first) {
      downloaded_targets.push_back(result.second);
    }
  }
  if (!targets.empty()) {
    if (targets.size() == downloaded_targets.size()) {
      sendDownloadReport();
      sendEvent<event::AllDownloadsComplete>(downloaded_targets);
    } else {
      LOG_ERROR << "Only " << downloaded_targets.size() << " of " << targets.size()
                << " were successfully downloaded. Report not sent.";
      sendEvent<event::Error>("Partial download");
      return false;
    }
  } else {
    sendEvent<event::NothingToDownload>();
    LOG_INFO << "No new updates to download.";
  }
  return true;
}

std::pair<bool, Uptane::Target> SotaUptaneClient::downloadImage(Uptane::Target target) {
  // TODO: support downloading encrypted targets from director
  // TODO: check if the file is already there before downloading
  if (!uptane_fetcher->fetchVerifyTarget(target)) {
    sendEvent<event::Error>("Error downloading targets.");
    return {false, target};
  }
  sendEvent<event::DownloadTargetComplete>(target);
  return {true, target};
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

void SotaUptaneClient::fetchMeta() {
  if (updateMeta()) {
    sendEvent<event::FetchMetaComplete>();
  } else {
    sendEvent<event::Error>("Could not update metadata.");
  }
}

void SotaUptaneClient::checkUpdates() {
  AssembleManifest();  // populates list of connected devices and installed images
  std::vector<Uptane::Target> updates;
  unsigned int ecus_count = 0;
  if (!uptaneOfflineIteration(&updates, &ecus_count)) {
    LOG_ERROR << "Invalid UPTANE metadata in storage";
  } else {
    if (!updates.empty()) {
      sendEvent<event::UpdateAvailable>(updates, ecus_count);
    } else {
      sendEvent<event::NoUpdateAvailable>();
      LOG_INFO << "No new updates found in Uptane metadata.";
    }
  }
}

void SotaUptaneClient::uptaneInstall(const std::vector<Uptane::Target> &updates) {
  // Uptane step 5 (send time to all ECUs) is not implemented yet.
  std::vector<Uptane::Target> primary_updates = findForEcu(updates, uptane_manifest.getPrimaryEcuSerial());
  //   6 - send metadata to all the ECUs
  sendMetadataToEcus(updates);

  //   7 - send images to ECUs (deploy for OSTree)
  if (primary_updates.size() != 0u) {
    // assuming one OSTree OS per primary => there can be only one OSTree update
    const Uptane::Target &primary_update = primary_updates[0];
    if (!isInstalledOnPrimary(primary_update)) {
      // notify the bootloader before installation happens, because installation is not atomic and
      //   a false notification doesn't hurt when rollbacks are implemented
      bootloader->updateNotify();
      sendEvent<event::InstallStarted>(uptane_manifest.getPrimaryEcuSerial());
      PackageInstallSetResult(primary_update);
      sendEvent<event::InstallTargetComplete>(uptane_manifest.getPrimaryEcuSerial(), true);
    } else {
      data::InstallOutcome outcome(data::UpdateResultCode::kAlreadyProcessed, "Package already installed");
      data::OperationResult result(primary_update.filename(), outcome);
      storage->storeInstallationResult(result);
      sendEvent<event::InstallStarted>(uptane_manifest.getPrimaryEcuSerial());
      sendEvent<event::InstallTargetComplete>(uptane_manifest.getPrimaryEcuSerial(), false);
    }
    // TODO: other updates for primary
  } else {
    LOG_INFO << "No update to install on primary";
  }

  sendImagesToEcus(updates);
}

void SotaUptaneClient::campaignCheck() {
  auto campaigns = campaign::fetchAvailableCampaigns(*http, config.tls.server);
  for (const auto &c : campaigns) {
    LOG_INFO << "Campaign: " << c.name;
    LOG_INFO << "Campaign id: " << c.id;
    LOG_INFO << "CampaignAccept required: " << (c.autoAccept ? "no" : "yes");
    LOG_INFO << "Message: " << c.description;
  }
  sendEvent<event::CampaignCheckComplete>(std::move(campaigns));
}

void SotaUptaneClient::campaignAccept(const std::string &campaign_id) {
  auto report_array = Json::Value(Json::arrayValue);
  auto report = std_::make_unique<Json::Value>();
  (*report)["id"] = Utils::randomUuid();
  (*report)["deviceTime"] = Uptane::TimeStamp::Now().ToString();
  (*report)["eventType"]["id"] = "campaign_accepted";
  (*report)["eventType"]["version"] = 0;
  (*report)["event"]["campaignId"] = campaign_id;
  report_queue->enqueue(std::move(report));
  sendEvent<event::CampaignAcceptComplete>();
}

void SotaUptaneClient::sendDownloadReport() {
  std::string director_targets;
  if (!storage->loadNonRoot(&director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets())) {
    LOG_ERROR << "Unable to load director targets metadata";
    return;
  }
  auto report = std_::make_unique<Json::Value>();
  (*report)["id"] = Utils::randomUuid();
  (*report)["deviceTime"] = Uptane::TimeStamp::Now().ToString();
  (*report)["eventType"]["id"] = "AllDownloadsComplete";
  (*report)["eventType"]["version"] = 1;
  (*report)["event"] = director_targets;

  report_queue->enqueue(std::move(report));
}

bool SotaUptaneClient::putManifestSimple() {
  // does not send event, so it can be used as a subset of other steps
  auto manifest = AssembleManifest();
  if (!hasPendingUpdates(manifest)) {
    auto signed_manifest = uptane_manifest.signManifest(manifest);
    HttpResponse response = http->put(config.uptane.director_server + "/manifest", signed_manifest);
    if (response.isOk()) {
      storage->clearInstallationResult();
      return true;
    }
  }
  return false;
}

void SotaUptaneClient::putManifest() {
  if (putManifestSimple()) {
    sendEvent<event::PutManifestComplete>();
  } else {
    sendEvent<event::Error>("Could not put manifest.");
  }
}

// Check stored secondaries list against secondaries known to aktualizr via
// commandline input and legacy interface.
void SotaUptaneClient::verifySecondaries() {
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

  int sec_root_version = secondary.getRootVersion((repo == Uptane::RepositoryType::Director));
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
      if (!secondary.putRoot(root, repo == Uptane::RepositoryType::Director)) {
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
  if (!storage->loadLatestRoot(&meta.director_root, Uptane::RepositoryType::Director)) {
    LOG_ERROR << "No director root metadata to send";
    return;
  }
  if (!storage->loadNonRoot(&meta.director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets())) {
    LOG_ERROR << "No director targets metadata to send";
    return;
  }
  if (!storage->loadLatestRoot(&meta.image_root, Uptane::RepositoryType::Images)) {
    LOG_ERROR << "No images root metadata to send";
    return;
  }
  if (!storage->loadNonRoot(&meta.image_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp())) {
    LOG_ERROR << "No images timestamp metadata to send";
    return;
  }
  if (!storage->loadNonRoot(&meta.image_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot())) {
    LOG_ERROR << "No images snapshot metadata to send";
    return;
  }
  if (!storage->loadNonRoot(&meta.image_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets())) {
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
        rotateSecondaryRoot(Uptane::RepositoryType::Director, *(sec->second));
        rotateSecondaryRoot(Uptane::RepositoryType::Images, *(sec->second));
        if (!sec->second->putMetadata(meta)) {
          LOG_ERROR << "Sending metadata to " << sec->second->getSerial() << " failed";
        }
      }
    }
  }
}

void SotaUptaneClient::waitAllInstallsComplete(std::vector<std::future<bool>> firmwareFutures) {
  for (auto &f : firmwareFutures) {
    f.wait();
  }
  sendEvent<event::AllInstallsComplete>();
}

void SotaUptaneClient::sendImagesToEcus(const std::vector<Uptane::Target> &targets) {
  std::vector<std::future<bool>> firmwareFutures;

  // target images should already have been downloaded to metadata_path/targets/
  for (auto targets_it = targets.cbegin(); targets_it != targets.cend(); ++targets_it) {
    for (auto ecus_it = targets_it->ecus().cbegin(); ecus_it != targets_it->ecus().cend(); ++ecus_it) {
      const Uptane::EcuSerial ecu_serial = ecus_it->first;

      auto sec = secondaries.find(ecu_serial);
      if (sec == secondaries.end()) {
        continue;
      }

      if (sec->second->sconfig.secondary_type == Uptane::SecondaryType::kOpcuaUptane) {
        Json::Value data;
        data["sysroot_path"] = config.pacman.sysroot.string();
        data["ref_hash"] = targets_it->sha256Hash();
        firmwareFutures.push_back(
            sec->second->sendFirmwareAsync(std::make_shared<std::string>(Utils::jsonToStr(data))));
        continue;
      }

      if (targets_it->IsOstree()) {
        // empty firmware means OSTree secondaries: pack credentials instead
        const std::string creds_archive = secondaryTreehubCredentials();
        if (creds_archive.empty()) {
          continue;
        }
        firmwareFutures.push_back(sec->second->sendFirmwareAsync(std::make_shared<std::string>(creds_archive)));
      } else {
        std::stringstream sstr;
        sstr << *storage->openTargetFile(targets_it->filename());
        const std::string fw = sstr.str();
        firmwareFutures.push_back(sec->second->sendFirmwareAsync(std::make_shared<std::string>(fw)));
      }
    }
  }

  std::async(std::launch::async, &SotaUptaneClient::waitAllInstallsComplete, this, std::move(firmwareFutures));
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
