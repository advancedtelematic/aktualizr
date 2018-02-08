#include "sotauptaneclient.h"

#include <unistd.h>

#include <boost/make_shared.hpp>
#include "json/json.h"

#include "crypto.h"
#include "logging.h"
#include "packagemanagerfactory.h"
#include "uptane/cryptokey.h"
#include "uptane/exceptions.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryfactory.h"
#include "utils.h"

SotaUptaneClient::SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in, Uptane::Repository &repo,
                                   const boost::shared_ptr<INvStorage> storage_in, HttpInterface &http_client)
    : config(config_in),
      events_channel(events_channel_in),
      uptane_repo(repo),
      storage(storage_in),
      http(http_client),
      last_targets_version(-1) {
  pacman = PackageManagerFactory::makePackageManager(config.pacman);
  initSecondaries();
}

void SotaUptaneClient::run(command::Channel *commands_channel) {
  while (true) {
    *commands_channel << boost::make_shared<command::GetUpdateRequests>();
    boost::this_thread::sleep_for(boost::chrono::seconds(config.uptane.polling_sec));
  }
}

bool SotaUptaneClient::isInstalled(const Uptane::Target &target) {
  if (target.ecu_identifier() == uptane_repo.getPrimaryEcuSerial()) {
    return target.sha256Hash() == pacman->getCurrent();
  } else {
    std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::const_iterator map_it =
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
  if (isInstalled(target)) {
    data::InstallOutcome outcome(data::ALREADY_PROCESSED, "Package already installed");
    operation_result["operation_result"] = data::OperationResult::fromOutcome(target.filename(), outcome).toJson();
  } else if ((!target.format().empty() && target.format() != "OSTREE") || target.length() != 0) {
    data::InstallOutcome outcome(data::VALIDATION_FAILED, "Cannot install a non-OSTree package on an OSTree system");
    operation_result["operation_result"] = data::OperationResult::fromOutcome(target.filename(), outcome).toJson();
  } else {
    data::OperationResult result = data::OperationResult::fromOutcome(target.filename(), PackageInstall(target));
    operation_result["operation_result"] = result.toJson();
    if (result.result_code == data::OK) {
      uptane_repo.saveInstalledVersion(target);
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
  Json::Value result = Json::arrayValue;
  std::string hash = pacman->getCurrent();
  std::string refname = uptane_repo.findInstalledVersion(hash);
  if (refname.empty()) {
    (refname += "unknown-") += hash;
  }
  Json::Value installed_image;
  installed_image["filepath"] = refname;
  installed_image["fileinfo"]["length"] = 0;
  installed_image["fileinfo"]["hashes"]["sha256"] = hash;

  Json::Value unsigned_ecu_version;
  unsigned_ecu_version["attacks_detected"] = "";
  unsigned_ecu_version["installed_image"] = installed_image;
  unsigned_ecu_version["ecu_serial"] = uptane_repo.getPrimaryEcuSerial();
  unsigned_ecu_version["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
  unsigned_ecu_version["timeserver_time"] = "1970-01-01T00:00:00Z";
  if (operation_result != Json::nullValue) {
    unsigned_ecu_version["custom"] = operation_result;
  }

  result.append(uptane_repo.signVersionManifest(unsigned_ecu_version));
  std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::iterator it;
  for (it = secondaries.begin(); it != secondaries.end(); it++) {
    Json::Value secmanifest = it->second->getManifest();
    if (secmanifest != Json::nullValue) result.append(secmanifest);
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

  boost::thread polling_thread(boost::bind(&SotaUptaneClient::run, this, commands_channel));

  boost::shared_ptr<command::BaseCommand> command;
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
          *events_channel << boost::make_shared<event::UptaneTimestampUpdated>();
          continue;
        }
        // Uptane step 4 - download all the images and verify them against the metadata (for OSTree - pull without
        // deploying)
        std::pair<int, std::vector<Uptane::Target> > updates = uptane_repo.getTargets();
        if (updates.second.size() && updates.first > last_targets_version) {
          LOG_INFO << "got new updates";
          *events_channel << boost::make_shared<event::UptaneTargetsUpdated>(updates.second);
          last_targets_version = updates.first;  // TODO: What if we fail install targets?
        } else {
          LOG_INFO << "no new updates, sending UptaneTimestampUpdated event";
          *events_channel << boost::make_shared<event::UptaneTimestampUpdated>();
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
            // treat empty format as OSTree for backwards compatibility
            // TODO: isInstalled gets called twice here, since
            // PackageInstallSetResult calls it too.
            if ((it->format().empty() || it->format() == "OSTREE") && !isInstalled(*it)) {
              PackageInstallSetResult(*it);
              break;
            }
          }
          // TODO: other updates for primary
        }
        sendImagesToEcus(updates);
        // Not required for Uptane, but used to send a status code to the
        // director.
        uptane_repo.putManifest(AssembleManifest());

      } else if (command->variant == "Shutdown") {
        polling_thread.interrupt();
        polling_thread.join();
        return;
      }

    } catch (Uptane::Exception e) {
      LOG_ERROR << e.what();
    } catch (const std::exception &ex) {
      LOG_ERROR << "Unknown exception was thrown: " << ex.what();
    }
  }
}

void SotaUptaneClient::initSecondaries() {
  std::vector<Uptane::SecondaryConfig>::const_iterator it;
  for (it = config.uptane.secondary_configs.begin(); it != config.uptane.secondary_configs.end(); ++it) {
    boost::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(*it);
    std::string sec_serial = sec->getSerial();
    std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::const_iterator map_it =
        secondaries.find(sec_serial);
    if (map_it != secondaries.end()) {
      LOG_ERROR << "Multiple secondaries found with the same serial: " << sec_serial;
      continue;
    }
    secondaries[sec_serial] = sec;
    uptane_repo.addSecondary(sec_serial, sec->getHwId(), sec->getPublicKey());
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
  // Should be a const_iterator but we need C++11 for cbegin.
  std::vector<std::pair<std::string, std::string> >::iterator store_it;
  store_it = std::find_if(serials.begin(), serials.end(), primary_comp);
  if (store_it == serials.end()) {
    LOG_ERROR << "Primary ECU serial " << uptane_repo.getPrimaryEcuSerial() << " not found in storage!";
    misconfigured_ecus.push_back(MisconfiguredEcu(store_it->first, store_it->second, kOld));
  } else {
    found[std::distance(serials.begin(), store_it)] = true;
  }

  std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::const_iterator it;
  for (it = secondaries.begin(); it != secondaries.end(); ++it) {
    SerialCompare secondary_comp(it->second->getSerial());
    store_it = std::find_if(serials.begin(), serials.end(), secondary_comp);
    if (store_it == serials.end()) {
      LOG_ERROR << "Secondary ECU serial " << it->second->getSerial() << " (hardware ID " << it->second->getHwId()
                << ") not found in storage!";
      misconfigured_ecus.push_back(MisconfiguredEcu(it->second->getSerial(), it->second->getHwId(), kNotRegistered));
    } else if (found[std::distance(serials.begin(), store_it)] == true) {
      LOG_ERROR << "Secondary ECU serial " << it->second->getSerial() << " (hardware ID " << it->second->getHwId()
                << ") has a duplicate entry in storage!";
    } else {
      found[std::distance(serials.begin(), store_it)] = true;
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

  Uptane::MetaPack meta;
  if (!uptane_repo.currentMeta(&meta)) {
    // couldn't get current metadata package, can't proceed
    return;
  }
  // target images should already have been downloaded to metadata_path/targets/
  for (it = targets.begin(); it != targets.end(); ++it) {
    std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::iterator sec =
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
    std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::iterator sec =
        secondaries.find(it->ecu_identifier());
    if (sec != secondaries.end()) {
      std::string firmware_path = uptane_repo.getTargetPath(*it);
      std::stringstream sstr;
      sstr << *storage->openTargetFile(firmware_path);
      sec->second->sendFirmware(sstr.str());
    }
  }
}
