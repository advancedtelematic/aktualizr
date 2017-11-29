#include "sotauptaneclient.h"

#include <unistd.h>

#include <boost/make_shared.hpp>
#include "json/json.h"

#include "crypto.h"
#include "httpclient.h"
#include "logger.h"
#include "uptane/exceptions.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryfactory.h"
#include "utils.h"

SotaUptaneClient::SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in, Uptane::Repository &repo)
    : config(config_in), events_channel(events_channel_in), uptane_repo(repo), last_targets_version(-1) {
  std::vector<Uptane::SecondaryConfig>::iterator it;
  for (it = config.uptane.secondary_configs.begin(); it != config.uptane.secondary_configs.end(); ++it) {
    std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::const_iterator map_it =
        secondaries.find(it->ecu_serial);
    if (map_it != secondaries.end()) {
      LOGGER_LOG(LVL_error, "Multiple secondaries found with the same serial: " << it->ecu_serial);
      continue;
    }
    secondaries[it->ecu_serial] = Uptane::SecondaryFactory::makeSecondary(*it);
    uptane_repo.addSecondary(it->ecu_serial, secondaries[it->ecu_serial]->getHwId(),
                             secondaries[it->ecu_serial]->getPublicKey());
  }
}

void SotaUptaneClient::run(command::Channel *commands_channel) {
  while (true) {
    *commands_channel << boost::make_shared<command::GetUpdateRequests>();
    boost::this_thread::sleep_for(boost::chrono::seconds(config.uptane.polling_sec));
  }
}

bool SotaUptaneClient::isInstalled(const Uptane::Target &target) {
  if (target.ecu_identifier() == uptane_repo.getPrimaryEcuSerial()) {
    return target.sha256Hash() == OstreePackage::getCurrent(config.ostree.sysroot);
  } else {
    std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::const_iterator map_it =
        secondaries.find(target.ecu_identifier());
    if (map_it != secondaries.end()) {
      // TODO: compare version
      return true;
    } else {
      // TODO: iterate through secondaries, compare version when found, throw exception otherwise
      LOGGER_LOG(LVL_error, "Multiple secondaries found with the same serial: " << map_it->second->getSerial());
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

data::InstallOutcome SotaUptaneClient::OstreeInstall(const Uptane::Target &target) {
  try {
    OstreePackage package(target.filename(), boost::algorithm::to_lower_copy(target.sha256Hash()),
                          config.uptane.ostree_server);

    data::PackageManagerCredentials cred;
    // All three files should live until package.install terminates
    TemporaryFile tmp_ca_file("ostree-ca");
    TemporaryFile tmp_pkey_file("ostree-pkey");
    TemporaryFile tmp_cert_file("ostree-cert");

    std::string ca;
    if (!uptane_repo.storage.loadTlsCa(&ca)) return data::InstallOutcome(data::INSTALL_FAILED, "CA file is absent");

    tmp_ca_file.PutContents(ca);

    cred.ca_file = tmp_ca_file.Path().native();
#ifdef BUILD_P11
    if (config.tls.pkey_source == kPkcs11)
      cred.pkey_file = uptane_repo.pkcs11_tls_keyname;
    else {
      std::string pkey;
      if (!uptane_repo.storage.loadTlsPkey(&pkey))
        return data::InstallOutcome(data::INSTALL_FAILED, "TLS primary key is absent");
      tmp_pkey_file.PutContents(pkey);
      cred.pkey_file = tmp_pkey_file.Path().native();
    }

    if (config.tls.cert_source == kPkcs11)
      cred.cert_file = uptane_repo.pkcs11_tls_certname;
    else {
      std::string cert;
      if (!uptane_repo.storage.loadTlsCert(&cert))
        return data::InstallOutcome(data::INSTALL_FAILED, "TLS certificate is absent");
      tmp_cert_file.PutContents(cert);
      cred.cert_file = tmp_cert_file.Path().native();
    }
#else
    std::string pkey;
    std::string cert;
    if (!uptane_repo.storage.loadTlsCert(&cert))
      return data::InstallOutcome(data::INSTALL_FAILED, "TLS certificate is absent");
    if (!uptane_repo.storage.loadTlsPkey(&pkey))
      return data::InstallOutcome(data::INSTALL_FAILED, "TLS primary key is absent");
    tmp_pkey_file.PutContents(pkey);
    tmp_cert_file.PutContents(cert);
    cred.pkey_file = tmp_pkey_file.Path().native();
    cred.cert_file = tmp_cert_file.Path().native();
#endif
    return package.install(cred, config.ostree, uptane_repo.getPrimaryHardwareId());
  } catch (std::exception &ex) {
    return data::InstallOutcome(data::INSTALL_FAILED, ex.what());
  }
}

void SotaUptaneClient::OstreeInstallSetResult(const Uptane::Target &target) {
  if (isInstalled(target)) {
    data::InstallOutcome outcome(data::ALREADY_PROCESSED, "Package already installed");
    operation_result["operation_result"] = data::OperationResult::fromOutcome(target.filename(), outcome).toJson();
  } else if ((!target.format().empty() && target.format() != "OSTREE") || target.length() != 0) {
    data::InstallOutcome outcome(data::VALIDATION_FAILED, "Cannot install a non-OSTree package on an OSTree system");
    operation_result["operation_result"] = data::OperationResult::fromOutcome(target.filename(), outcome).toJson();
  } else {
    data::OperationResult result = data::OperationResult::fromOutcome(target.filename(), OstreeInstall(target));
    operation_result["operation_result"] = result.toJson();
    if (result.result_code == data::OK) {
      uptane_repo.saveInstalledVersion(target);
    }
  }
}

void SotaUptaneClient::reportHwInfo() {
  Json::Value hw_info = Utils::getHardwareInfo();
  if (!hw_info.empty()) uptane_repo.http.put(config.tls.server + "/core/system_info", hw_info);
}

void SotaUptaneClient::reportInstalledPackages() {
  uptane_repo.http.put(config.tls.server + "/core/installed",
                       Ostree::getInstalledPackages(config.ostree.packages_file));
}

Json::Value SotaUptaneClient::AssembleManifest() {
  Json::Value result = Json::arrayValue;
  std::string hash = OstreePackage::getCurrent(config.ostree.sysroot);
  std::string refname = uptane_repo.findInstalledVersion(hash);
  if (refname.empty()) {
    (refname += "unknown-") += hash;
  }
  Json::Value unsigned_ecu_version =
      OstreePackage(refname, hash, "").toEcuVersion(uptane_repo.getPrimaryEcuSerial(), operation_result);

  result.append(uptane_repo.signVersionManifest(unsigned_ecu_version));
  std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::iterator it;
  for (it = secondaries.begin(); it != secondaries.end(); it++) {
    Json::Value secmanifest = it->second->getManifest();
    if (secmanifest != Json::nullValue) result.append(secmanifest);
  }
  return result;
}

void SotaUptaneClient::runForever(command::Channel *commands_channel) {
  LOGGER_LOG(LVL_debug, "Checking if device is provisioned...");

  if (!uptane_repo.initialize()) {
    throw std::runtime_error("Fatal error of tls or ecu device registration");
  }
  LOGGER_LOG(LVL_debug, "... provisioned OK");
  reportHwInfo();
  reportInstalledPackages();

  boost::thread polling_thread(boost::bind(&SotaUptaneClient::run, this, commands_channel));

  boost::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    LOGGER_LOG(LVL_info, "got " + command->variant + " command");

    try {
      if (command->variant == "GetUpdateRequests") {
        // Uptane step 1 (build the vehicle version manifest):
        uptane_repo.putManifest(AssembleManifest());
        // Uptane step 2 (download time) is not implemented yet.
        // Uptane steps 3 and 4 (download metadata and then images):
        std::pair<int, std::vector<Uptane::Target> > updates = uptane_repo.getTargets();
        if (updates.second.size() && updates.first > last_targets_version) {
          LOGGER_LOG(LVL_info, "got new updates");
          *events_channel << boost::make_shared<event::UptaneTargetsUpdated>(updates.second);
          last_targets_version = updates.first;  // TODO: What if we fail install targets?
        } else {
          LOGGER_LOG(LVL_info, "no new updates, sending UptaneTimestampUpdated event");
          *events_channel << boost::make_shared<event::UptaneTimestampUpdated>();
        }
      } else if (command->variant == "UptaneInstall") {
        operation_result = Json::nullValue;
        std::vector<Uptane::Target> updates = command->toChild<command::UptaneInstall>()->packages;
        std::vector<Uptane::Target> primary_updates = findForEcu(updates, uptane_repo.getPrimaryEcuSerial());
        // TODO: both primary and secondary updates should be split into sequence of stages specified by UPTANE:
        //   4 - download all the images and verify them against the metadata (for OSTree - pull without deploying)
        //   6 - send metadata to all the ECUs
        //   7 - send images to ECUs (deploy for OSTree)
        // Uptane step 5 (send time to all ECUs) is not implemented yet.
        updateSecondaries(updates);
        if (primary_updates.size()) {
          // assuming one OSTree OS per primary => there can be only one OSTree update
          std::vector<Uptane::Target>::const_iterator it;
          for (it = primary_updates.begin(); it != primary_updates.end(); ++it) {
            // treat empty format as OSTree for backwards compatibility
            // TODO: isInstalled gets called twice here, since
            // OstreeInstallSetResult calls it too.
            if ((it->format().empty() || it->format() == "OSTREE") && !isInstalled(*it)) {
              OstreeInstallSetResult(*it);
              break;
            }
          }
          // TODO: other updates for primary
        }
        // Not required for Uptane, but used to send a status code to the
        // director.
        uptane_repo.putManifest(AssembleManifest());

      } else if (command->variant == "Shutdown") {
        polling_thread.interrupt();
        polling_thread.join();
        return;
      }

    } catch (Uptane::Exception e) {
      LOGGER_LOG(LVL_error, e.what());
    } catch (const std::exception &ex) {
      LOGGER_LOG(LVL_error, "Unknown exception was thrown: " << ex.what());
    }
  }
}

// TODO: the function can't currently return any errors. The problem of error reporting from secondaries should be
// solved on a system (backend+frontend) error.
// TODO: the function blocks until it updates all the secondaries. Consider non-blocking operation.
void SotaUptaneClient::updateSecondaries(std::vector<Uptane::Target> targets) {
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
      std::string firmware_path = uptane_repo.image.getTargetPath(*it);
      if (!boost::filesystem::exists(firmware_path)) continue;

      if (!sec->second->putMetadata(meta)) {
        // connection error while putting metadata, can't upload firmware
        continue;
      }

      std::string firmware = Utils::readFile(firmware_path);

      sec->second->sendFirmware(firmware);
    }
  }
}
