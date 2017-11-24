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
      // compare version
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

Json::Value SotaUptaneClient::OstreeInstallAndManifest(const Uptane::Target &target) {
  Json::Value operation_result;
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
  Json::Value unsigned_ecu_version =
      OstreePackage(target.filename(), target.sha256Hash(), "").toEcuVersion(target.ecu_identifier(), operation_result);

  ENGINE *crypto_engine = NULL;
#ifdef BUILD_P11
  if ((uptane_repo.key_source == kPkcs11)) crypto_engine = uptane_repo.p11.getEngine();
#endif

  Json::Value ecu_version_signed = Crypto::signTuf(crypto_engine, uptane_repo.primary_private_key,
                                                   uptane_repo.primary_public_key_id, unsigned_ecu_version);
  return ecu_version_signed;
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
      OstreePackage(refname, hash, "").toEcuVersion(uptane_repo.getPrimaryEcuSerial(), Json::nullValue);

  result.append(uptane_repo.getCurrentVersionManifests(unsigned_ecu_version));
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
        uptane_repo.putManifest(AssembleManifest());
        // Steps 1, 3, 4 (only non-OSTree) of UPTANE 8.1, step 2 (time) is not implemented yet
        std::pair<int, std::vector<Uptane::Target> > updates = uptane_repo.getTargets();
        if (updates.second.size() && updates.first > last_targets_version) {
          LOGGER_LOG(LVL_info, "got new updates");
          *events_channel << boost::make_shared<event::UptaneTargetsUpdated>(updates.second);
          last_targets_version = updates.first;  // What if we fail install targets?
        } else {
          LOGGER_LOG(LVL_info, "no new updates, sending UptaneTimestampUpdated event");
          *events_channel << boost::make_shared<event::UptaneTimestampUpdated>();
        }
      } else if (command->variant == "UptaneInstall") {
        std::vector<Uptane::Target> updates = command->toChild<command::UptaneInstall>()->packages;
        std::vector<Uptane::Target> primary_updates = findForEcu(updates, uptane_repo.getPrimaryEcuSerial());
        Json::Value manifests(Json::arrayValue);
        // TODO: both primary and secondary updates should be split into sequence of stages specified by UPTANE:
        //   4 - download all the images and verify them against the metadata (for OSTree - pull without deploying)
        //   6 - send metadata to all the ECUs
        //   7 - send images to ECUs (deploy for OSTree)
        manifests = uptane_repo.updateSecondaries(updates);
        if (primary_updates.size()) {
          // assuming one OSTree OS per primary => there can be only one OSTree update
          std::vector<Uptane::Target>::const_iterator it;
          for (it = primary_updates.begin(); it != primary_updates.end(); ++it) {
            // treat empty format as OSTree for backwards compatibility
            // TODO: isInstalled gets called twice here, since
            // OstreeInstallAndManifest calls it too.
            if ((it->format().empty() || it->format() == "OSTREE") && !isInstalled(*it)) {
              Json::Value p_manifest = OstreeInstallAndManifest(*it);
              manifests.append(p_manifest);
              break;
            }
          }
          // TODO: other updates for primary
        }
        // TODO: this step seems to be not required by UPTANE
        uptane_repo.putManifest(manifests);

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
