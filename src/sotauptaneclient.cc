#include "sotauptaneclient.h"

#include <unistd.h>
#include <boost/make_shared.hpp>

#include "crypto.h"
#include "httpclient.h"
#include "json/json.h"
#include "logger.h"
#include "uptane/exceptions.h"
#include "utils.h"

SotaUptaneClient::SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in, Uptane::Repository &repo)
    : config(config_in), events_channel(events_channel_in), uptane_repo(repo), last_targets_version(-1) {}

void SotaUptaneClient::run(command::Channel *commands_channel) {
  while (true) {
    *commands_channel << boost::make_shared<command::GetUpdateRequests>();
    boost::this_thread::sleep_for(boost::chrono::seconds(config.uptane.polling_sec));
  }
}

bool SotaUptaneClient::isInstalled(const Uptane::Target &target) {
  if (target.ecu_identifier() == uptane_repo.getPrimaryEcuSerial()) {
    return target.filename() ==
           OstreePackage::getEcu(uptane_repo.getPrimaryEcuSerial(), config.ostree.sysroot).ref_name;
  } else {
    // TODO: iterate through secondaries, compare version when found, throw exception otherwise
    return true;
  }
}

std::vector<Uptane::Target> SotaUptaneClient::findForEcu(const std::vector<Uptane::Target> &targets,
                                                         std::string ecu_id) {
  std::vector<Uptane::Target> result;
  for (std::vector<Uptane::Target>::const_iterator it = targets.begin(); it != targets.end(); ++it) {
    if (it->ecu_identifier() == ecu_id) {
      result.push_back(*it);
    }
  }
  return result;
}

data::InstallOutcome SotaUptaneClient::OstreeInstall(const Uptane::Target &target) {
  std::size_t pos = target.filename().find_last_of("-");
  if (pos == std::string::npos) return data::InstallOutcome(data::INSTALL_FAILED, "Invalid refname");

  std::string branch_name = target.filename().substr(0, pos);
  std::string refhash = target.filename().substr(pos + 1, std::string::npos);
  if (branch_name.empty() || refhash.empty()) return data::InstallOutcome(data::INSTALL_FAILED, "Invalid refname");

  OstreePackage package(target.ecu_identifier(), target.filename(), branch_name, refhash, "",
                        config.uptane.ostree_server);
  // TODO: use storage
  data::PackageManagerCredentials cred;
  cred.ca_file = (config.tls.certificates_directory / config.tls.ca_file).string();
  cred.pkey_file = (config.tls.certificates_directory / config.tls.pkey_file).string();
  cred.cert_file = (config.tls.certificates_directory / config.tls.client_certificate).string();
  return package.install(cred, config.ostree);
}

Json::Value SotaUptaneClient::OstreeInstallAndManifest(const Uptane::Target &target) {
  data::InstallOutcome outcome = OstreeInstall(target);

  data::OperationResult result = data::OperationResult::fromOutcome(target.filename(), outcome);
  Json::Value operation_result;
  operation_result["operation_result"] = result.toJson();
  Json::Value unsigned_ecu_version =
      OstreePackage::getEcu(uptane_repo.getPrimaryEcuSerial(), config.ostree.sysroot).toEcuVersion(operation_result);
  Json::Value ecu_version_signed =
      Crypto::signTuf(uptane_repo.primary_private_key, uptane_repo.primary_public_key, unsigned_ecu_version);
  return ecu_version_signed;
}

void SotaUptaneClient::reportHWInfo() {
  Json::Value hw_info = Utils::getHardwareInfo();
  if (!hw_info.empty()) uptane_repo.http.put(config.tls.server + "/core/system_info", hw_info);
}

void SotaUptaneClient::reportInstalledPackages() {
  uptane_repo.http.put(config.tls.server + "/core/installed",
                       Ostree::getInstalledPackages(config.ostree.packages_file));
}

void SotaUptaneClient::runForever(command::Channel *commands_channel) {
  LOGGER_LOG(LVL_debug, "Checking if device is provisioned...");
  if (!uptane_repo.initialize()) {
    throw std::runtime_error("Fatal error of tls or ecu device registration");
  }
  LOGGER_LOG(LVL_debug, "... provisioned OK");
  reportHWInfo();
  reportInstalledPackages();

  boost::thread polling_thread(boost::bind(&SotaUptaneClient::run, this, commands_channel));

  boost::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    LOGGER_LOG(LVL_info, "got " + command->variant + " command");

    try {
      if (command->variant == "GetUpdateRequests") {
        Json::Value unsigned_ecu_version =
            OstreePackage::getEcu(uptane_repo.getPrimaryEcuSerial(), config.ostree.sysroot)
                .toEcuVersion(Json::nullValue);
        uptane_repo.putManifest(uptane_repo.getCurrentVersionManifests(unsigned_ecu_version));
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
        manifests = uptane_repo.updateSecondaries(updates);
        if (primary_updates.size()) {
          // assuming one OSTree OS per primary => there can be only one OSTree update
          for (std::vector<Uptane::Target>::const_iterator it = primary_updates.begin(); it != primary_updates.end();
               ++it) {
            // treat empty format as OSTree for backwards compatibility
            if ((it->format().empty() || it->format() == "OSTREE") && !isInstalled(*it)) {
              Json::Value p_manifest = OstreeInstallAndManifest(*it);
              manifests.append(p_manifest);
              break;
            }
          }
          // TODO: other updates for primary
        }
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
