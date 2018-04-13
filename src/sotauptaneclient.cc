#include "sotauptaneclient.h"

#include <unistd.h>
#include <chrono>
#include "json/json.h"

#include "crypto.h"
#include "keymanager.h"
#include "logging.h"
#include "packagemanagerfactory.h"
#include "uptane/exceptions.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryfactory.h"
#include "utils.h"

SotaUptaneClient::SotaUptaneClient(Config &config_in, event::Channel *events_channel_in, Uptane::Repository &repo,
                                   const std::shared_ptr<INvStorage> storage_in, HttpInterface &http_client)
    : config(config_in),
      events_channel(events_channel_in),
      uptane_repo(repo),
      storage(storage_in),
      http(http_client),
      last_targets_version(-1) {
  pacman = PackageManagerFactory::makePackageManager(config.pacman, storage);

  if (config.discovery.ipuptane) {
    IpSecondaryDiscovery ip_uptane_discovery{config.network};
    auto ipuptane_secs = ip_uptane_discovery.discover();
    config.uptane.secondary_configs.insert(config.uptane.secondary_configs.end(), ipuptane_secs.begin(),
                                           ipuptane_secs.end());
    ip_uptane_connection = std::unique_ptr<IpUptaneConnection>{new IpUptaneConnection{config.network.ipuptane_port}};
    ip_uptane_splitter =
        std::unique_ptr<IpUptaneConnectionSplitter>{new IpUptaneConnectionSplitter{*ip_uptane_connection}};
  }
  initSecondaries();
}

void SotaUptaneClient::run(command::Channel *commands_channel) {
  while (true) {
    *commands_channel << std::make_shared<command::GetUpdateRequests>();
    std::this_thread::sleep_for(std::chrono::seconds(config.uptane.polling_sec));
    if (shutdown) return;
  }
}

bool SotaUptaneClient::isInstalled(const Uptane::Target &target) {
  if (target.ecu_identifier() == uptane_repo.getPrimaryEcuSerial()) {
    return target == pacman->getCurrent();
  } else {
    std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >::const_iterator map_it =
        secondaries.find(target.ecu_identifier());
    if (map_it != secondaries.end()) {
      // TODO: compare version
      return true;
    } else {
      // TODO: iterate through secondaries, compare version when found, throw exception otherwise
      LOG_ERROR << "Multiple secondaries found with the same serial: " << map_it->second->getSerial();
      return false;
    }
  }
}

std::vector<Uptane::Target> SotaUptaneClient::findForEcu(const std::vector<Uptane::Target> &targets,
                                                         const std::string &ecu_id) {
  std::vector<Uptane::Target> result;
  for (std::vector<Uptane::Target>::const_iterator it = targets.begin(); it != targets.end(); ++it) {
    if (it->ecu_identifier() == ecu_id) {
      result.push_back(*it);
    }
  }
  return result;
}

data::InstallOutcome SotaUptaneClient::PackageInstall(const Uptane::Target &target) {
  try {
    return pacman->install(target);
  } catch (std::exception &ex) {
    return data::InstallOutcome(data::INSTALL_FAILED, ex.what());
  }
}

void SotaUptaneClient::PackageInstallSetResult(const Uptane::Target &target) {
  if (((!target.format().empty() && target.format() != "OSTREE") || target.length() != 0) &&
      config.pacman.type == kOstree) {
    data::InstallOutcome outcome(data::VALIDATION_FAILED, "Cannot install a non-OSTree package on an OSTree system");
    operation_result["operation_result"] = data::OperationResult::fromOutcome(target.filename(), outcome).toJson();
  } else {
    data::OperationResult result = data::OperationResult::fromOutcome(target.filename(), PackageInstall(target));
    operation_result["operation_result"] = result.toJson();
    if (result.result_code == data::OK) {
      storage->saveInstalledVersion(target);
    }
  }
}

void SotaUptaneClient::reportHwInfo() {
  Json::Value hw_info = Utils::getHardwareInfo();
  if (!hw_info.empty()) {
    http.put(config.tls.server + "/core/system_info", hw_info);
  }
}

void SotaUptaneClient::reportInstalledPackages() {
  http.put(config.tls.server + "/core/installed", pacman->getInstalledPackages());
}

Json::Value SotaUptaneClient::AssembleManifest() {
  Json::Value result;
  Json::Value unsigned_ecu_version = pacman->getManifest(uptane_repo.getPrimaryEcuSerial());

  if (operation_result != Json::nullValue) {
    unsigned_ecu_version["custom"] = operation_result;
  }

  result[uptane_repo.getPrimaryEcuSerial()] = uptane_repo.signVersionManifest(unsigned_ecu_version);
  std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >::iterator it;
  for (it = secondaries.begin(); it != secondaries.end(); it++) {
    Json::Value secmanifest = it->second->getManifest();
    if (secmanifest.isMember("signatures") && secmanifest.isMember("signed")) {
      auto public_key = it->second->getPublicKey();
      PublicKey public_key_object(public_key.second, keyTypeToString(public_key.first));
      std::string canonical = Json::FastWriter().write(secmanifest["signed"]);
      bool verified =
          Crypto::VerifySignature(public_key_object, secmanifest["signatures"][0]["sig"].asString(), canonical);
      if (verified) {
        result[it->first] = secmanifest;
      } else {
        LOG_ERROR << "Secondary manifest verification failed, manifest: " << secmanifest;
      }
    } else {
      LOG_ERROR << "Secondary manifest is corrupted or not signed, manifest: " << secmanifest;
    }
  }
  return result;
}

void SotaUptaneClient::runForever(command::Channel *commands_channel) {
  LOG_DEBUG << "Checking if device is provisioned...";

  if (!uptane_repo.initialize()) {
    throw std::runtime_error("Fatal error of tls or ecu device registration");
  }
  verifySecondaries();
  LOG_DEBUG << "... provisioned OK";
  reportHwInfo();
  reportInstalledPackages();

  std::thread polling_thread(std::bind(&SotaUptaneClient::run, this, commands_channel));

  std::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    LOG_INFO << "got " + command->variant + " command";

    try {
      if (command->variant == "GetUpdateRequests") {
        // Uptane step 1 (build the vehicle version manifest):
        uptane_repo.putManifest(AssembleManifest());
        // Uptane step 2 (download time) is not implemented yet.
        // Uptane step 3 (download metadata)
        if (!uptane_repo.getMeta()) {
          LOG_ERROR << "could not retrieve metadata";
          *events_channel << std::make_shared<event::UptaneTimestampUpdated>();
          continue;
        }
        // Uptane step 4 - download all the images and verify them against the metadata (for OSTree - pull without
        // deploying)
        std::pair<int, std::vector<Uptane::Target> > updates = uptane_repo.getTargets();
        if (updates.second.size() && updates.first > last_targets_version) {
          LOG_INFO << "got new updates";
          *events_channel << std::make_shared<event::UptaneTargetsUpdated>(updates.second);
          last_targets_version = updates.first;  // TODO: What if we fail install targets?
        } else {
          LOG_INFO << "no new updates, sending UptaneTimestampUpdated event";
          *events_channel << std::make_shared<event::UptaneTimestampUpdated>();
        }
      } else if (command->variant == "UptaneInstall") {
        // Uptane step 5 (send time to all ECUs) is not implemented yet.
        operation_result = Json::nullValue;
        std::vector<Uptane::Target> updates = command->toChild<command::UptaneInstall>()->packages;
        std::vector<Uptane::Target> primary_updates = findForEcu(updates, uptane_repo.getPrimaryEcuSerial());
        //   6 - send metadata to all the ECUs
        sendMetadataToEcus(updates);

        //   7 - send images to ECUs (deploy for OSTree)
        if (primary_updates.size()) {
          // assuming one OSTree OS per primary => there can be only one OSTree update
          std::vector<Uptane::Target>::const_iterator it;
          for (it = primary_updates.begin(); it != primary_updates.end(); ++it) {
            if (!isInstalled(*it)) {
              PackageInstallSetResult(*it);
            } else {
              data::InstallOutcome outcome(data::ALREADY_PROCESSED, "Package already installed");
              operation_result["operation_result"] =
                  data::OperationResult::fromOutcome(it->filename(), outcome).toJson();
            }
            break;
          }
          // TODO: other updates for primary
        }

        sendImagesToEcus(updates);
        // Not required for Uptane, but used to send a status code to the
        // director.
        uptane_repo.putManifest(AssembleManifest());

        boost::filesystem::path reboot_flag = "/tmp/aktualizr_reboot_flag";
        if (boost::filesystem::exists(reboot_flag)) {
          boost::filesystem::remove(reboot_flag);
          if (getppid() == 1) {  // if parent process id is 1, aktualizr runs under systemd
            exit(0);             // aktualizr service runs with 'Restart=always' systemd option.
                                 // Systemd will start aktualizr automatically if it exit.
          } else {               // Aktualizr runs from terminal
            LOG_INFO << "Aktualizr has been updated and reqires restart to run new version.";
          }
        }
      } else if (command->variant == "Shutdown") {
        shutdown = true;
        polling_thread.join();
        return;
      }

    } catch (Uptane::Exception e) {
      LOG_ERROR << e.what();
      *events_channel << std::make_shared<event::UptaneTimestampUpdated>();
    } catch (const std::exception &ex) {
      LOG_ERROR << "Unknown exception was thrown: " << ex.what();
      *events_channel << std::make_shared<event::UptaneTimestampUpdated>();
    }
  }
}

void SotaUptaneClient::initSecondaries() {
  std::vector<Uptane::SecondaryConfig>::const_iterator it;
  for (it = config.uptane.secondary_configs.begin(); it != config.uptane.secondary_configs.end(); ++it) {
    std::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(*it);
    if (it->secondary_type == Uptane::kIpUptane)
      ip_uptane_splitter->registerSecondary(*dynamic_cast<Uptane::IpUptaneSecondary *>(&(*sec)));

    std::string sec_serial = sec->getSerial();
    std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >::const_iterator map_it =
        secondaries.find(sec_serial);
    if (map_it != secondaries.end()) {
      LOG_ERROR << "Multiple secondaries found with the same serial: " << sec_serial;
      continue;
    }
    secondaries[sec_serial] = sec;
    uptane_repo.addSecondary(sec);
  }
}

// Check stored secondaries list against secondaries known to aktualizr via
// commandline input and legacy interface.
void SotaUptaneClient::verifySecondaries() {
  std::vector<std::pair<std::string, std::string> > serials;
  if (!storage->loadEcuSerials(&serials) || serials.empty()) {
    LOG_ERROR << "No ECU serials found in storage!";
    return;
  }

  std::vector<MisconfiguredEcu> misconfigured_ecus;
  std::vector<bool> found(serials.size(), false);
  SerialCompare primary_comp(uptane_repo.getPrimaryEcuSerial());
  std::vector<std::pair<std::string, std::string> >::const_iterator store_it;
  store_it = std::find_if(serials.begin(), serials.end(), primary_comp);
  if (store_it == serials.end()) {
    LOG_ERROR << "Primary ECU serial " << uptane_repo.getPrimaryEcuSerial() << " not found in storage!";
    misconfigured_ecus.push_back(MisconfiguredEcu(store_it->first, store_it->second, kOld));
  } else {
    found[std::distance(serials.cbegin(), store_it)] = true;
  }

  std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >::const_iterator it;
  for (it = secondaries.cbegin(); it != secondaries.cend(); ++it) {
    SerialCompare secondary_comp(it->second->getSerial());
    store_it = std::find_if(serials.begin(), serials.end(), secondary_comp);
    if (store_it == serials.end()) {
      LOG_ERROR << "Secondary ECU serial " << it->second->getSerial() << " (hardware ID " << it->second->getHwId()
                << ") not found in storage!";
      misconfigured_ecus.push_back(MisconfiguredEcu(it->second->getSerial(), it->second->getHwId(), kNotRegistered));
    } else if (found[std::distance(serials.cbegin(), store_it)] == true) {
      LOG_ERROR << "Secondary ECU serial " << it->second->getSerial() << " (hardware ID " << it->second->getHwId()
                << ") has a duplicate entry in storage!";
    } else {
      found[std::distance(serials.cbegin(), store_it)] = true;
    }
  }

  std::vector<bool>::iterator found_it;
  for (found_it = found.begin(); found_it != found.end(); ++found_it) {
    if (!*found_it) {
      std::pair<std::string, std::string> not_registered = serials[std::distance(found.begin(), found_it)];
      LOG_WARNING << "ECU serial " << not_registered.first << " in storage was not reported to aktualizr!";
      misconfigured_ecus.push_back(MisconfiguredEcu(not_registered.first, not_registered.second, kOld));
    }
  }
  storage->storeMisconfiguredEcus(misconfigured_ecus);
}

// TODO: the function can't currently return any errors. The problem of error reporting from secondaries should be
// solved on a system (backend+frontend) error.
// TODO: the function blocks until it updates all the secondaries. Consider non-blocking operation.
void SotaUptaneClient::sendMetadataToEcus(std::vector<Uptane::Target> targets) {
  std::vector<Uptane::Target>::const_iterator it;

  Uptane::MetaPack meta = uptane_repo.currentMeta();

  // target images should already have been downloaded to metadata_path/targets/
  for (it = targets.begin(); it != targets.end(); ++it) {
    std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >::iterator sec =
        secondaries.find(it->ecu_identifier());
    if (sec != secondaries.end()) {
      if (!sec->second->putMetadata(meta)) {
        // connection error while putting metadata
        continue;
      }
    }
  }
}

void SotaUptaneClient::sendImagesToEcus(std::vector<Uptane::Target> targets) {
  std::vector<Uptane::Target>::const_iterator it;
  // target images should already have been downloaded to metadata_path/targets/
  for (it = targets.begin(); it != targets.end(); ++it) {
    auto sec = secondaries.find(it->ecu_identifier());
    if (sec == secondaries.end()) {
      continue;
    }

    if (sec->second->sconfig.secondary_type == Uptane::kOpcuaUptane) {
      Json::Value data;
      data["sysroot_path"] = config.pacman.sysroot.string();
      data["ref_hash"] = it->sha256Hash();
      sec->second->sendFirmware(Utils::jsonToStr(data));
      continue;
    }

    std::stringstream sstr;
    sstr << *storage->openTargetFile(it->filename());
    std::string fw = sstr.str();

    if (fw.empty()) {
      // empty firmware means OSTree secondaries: pack credentials instead
      std::string creds_archive = secondaryTreehubCredentials();
      if (creds_archive.empty()) {
        continue;
      }
      sec->second->sendFirmware(creds_archive);
    } else {
      sec->second->sendFirmware(fw);
    }
  }
}

std::string SotaUptaneClient::secondaryTreehubCredentials() const {
  if (config.tls.pkey_source != kFile || config.tls.cert_source != kFile || config.tls.ca_source != kFile) {
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
