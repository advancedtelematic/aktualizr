#include "sotauptaneclient.h"

#include <unistd.h>
#include <boost/make_shared.hpp>

#include "crypto.h"
#include "httpclient.h"
#include "json/json.h"
#include "logger.h"
#include "uptane/exceptions.h"
#include "utils.h"

SotaUptaneClient::SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in)
    : config(config_in), events_channel(events_channel_in), uptane_repo(config) {}

void SotaUptaneClient::run(command::Channel *commands_channel) {
  while (true) {
    *commands_channel << boost::make_shared<command::GetUpdateRequests>();
    boost::this_thread::sleep_for(boost::chrono::seconds(config.uptane.polling_sec));
  }
}

bool SotaUptaneClient::isInstalled(const Uptane::Target &target) {
  if (target.ecu_identifier() == config.uptane.primary_ecu_serial) {
    return target.filename() == OstreePackage::getEcu(config.uptane.primary_ecu_serial, config.ostree.sysroot).ref_name;
  } else {
    // TODO: iterate throgh secondaries, compare version when found, throw exception otherwise
    return false;
  }
}

std::vector<Uptane::Target> SotaUptaneClient::findOstree(const std::vector<Uptane::Target> &targets) {
  std::vector<Uptane::Target> result;
  for (std::vector<Uptane::Target>::const_iterator it = targets.begin(); it != targets.end(); ++it) {
    // treat empty format as OSTree for backwards compatibility
    if (it->format().empty() || it->format() == "OSTREE") result.push_back(*it);
  }
  return result;
}

std::vector<Uptane::Target> SotaUptaneClient::findForEcu(const std::vector<Uptane::Target> &targets,
                                                         std::string ecu_id) {
  std::vector<Uptane::Target> result;
  for (std::vector<Uptane::Target>::const_iterator it = targets.begin(); it != targets.end(); ++it)
    if (it->ecu_identifier() == ecu_id) result.push_back(*it);
  return result;
}

// For now just returns vector on size 1 with the update for the primary. Next step is to move secondaries to
// SotaUptaneClient
std::vector<Uptane::Target> SotaUptaneClient::getUpdates() {
  std::vector<Uptane::Target> targets = uptane_repo.getTargets().second;
  std::vector<Uptane::Target> result;
  for (std::vector<Uptane::Target>::iterator it = targets.begin(); it != targets.end(); ++it)
    if (!isInstalled(*it)) result.push_back(*it);
  return result;
}

OstreePackage SotaUptaneClient::uptaneToOstree(const Uptane::Target &target) {
  return OstreePackage(target.ecu_identifier(), target.filename(), "", config.uptane.ostree_server);
}

void SotaUptaneClient::OstreeInstall(const OstreePackage &package) {
  data::PackageManagerCredentials cred;
  cred.ca_file = (config.tls.certificates_directory / config.tls.ca_file).string();
  cred.pkey_file = (config.tls.certificates_directory / config.tls.pkey_file).string();
  cred.cert_file = (config.tls.certificates_directory / config.tls.client_certificate).string();
  data::InstallOutcome outcome = package.install(cred, config.ostree);
  data::OperationResult result = data::OperationResult::fromOutcome(package.ref_name, outcome);
  Json::Value operation_result;
  operation_result["operation_result"] = result.toJson();
  uptane_repo.putManifest(operation_result);
}

void SotaUptaneClient::reportHWInfo() {
  HttpClient http;
  http.authenticate((config.tls.certificates_directory / config.tls.client_certificate).string(),
                    (config.tls.certificates_directory / config.tls.ca_file).string(),
                    (config.tls.certificates_directory / config.tls.pkey_file).string());

  Json::Value hw_info = Utils::getHardwareInfo();
  if (!hw_info.empty()) {
    http.put(config.tls.server + "/core/system_info", Utils::getHardwareInfo());
  }
}

void SotaUptaneClient::reportInstalledPackages() {
  HttpClient http;
  http.authenticate((config.tls.certificates_directory / config.tls.client_certificate).string(),
                    (config.tls.certificates_directory / config.tls.ca_file).string(),
                    (config.tls.certificates_directory / config.tls.pkey_file).string());

  http.put(config.tls.server + "/core/installed", Ostree::getInstalledPackages(config.ostree.packages_file));
}

void SotaUptaneClient::runForever(command::Channel *commands_channel) {
  LOGGER_LOG(LVL_debug, "Checking if device is provisioned...");
  if (!uptane_repo.deviceRegister() || !uptane_repo.ecuRegister()) {
    throw std::runtime_error("Fatal error of tls or ecu device registration");
  }
  LOGGER_LOG(LVL_debug, "... provisioned OK");
  reportHWInfo();
  reportInstalledPackages();
  uptane_repo.authenticate();
  boost::thread polling_thread(boost::bind(&SotaUptaneClient::run, this, commands_channel));

  boost::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    LOGGER_LOG(LVL_info, "got " + command->variant + " command");

    try {
      if (command->variant == "GetUpdateRequests") {
        std::vector<Uptane::Target> updates = getUpdates();
        if (updates.size()) {
          LOGGER_LOG(LVL_info, "got new updates");
          *events_channel << boost::make_shared<event::UptaneTargetsUpdated>(updates);
        } else {
          LOGGER_LOG(LVL_info, "no new updates, sending UptaneTimestampUpdated event");
          *events_channel << boost::make_shared<event::UptaneTimestampUpdated>();
        }
      } else if (command->variant == "UptaneInstall") {
        std::vector<Uptane::Target> updates = command->toChild<command::UptaneInstall>()->packages;
        std::vector<Uptane::Target> primary_updates = findForEcu(updates, config.uptane.primary_ecu_serial);
        if (primary_updates.size()) {
          // assuming one OSTree OS per primary => there can be only one OSTree update
          std::vector<Uptane::Target> main_ostree = findOstree(primary_updates);
          if (main_ostree.size()) OstreeInstall(uptaneToOstree(main_ostree[0]));

          // TODO: other updates for primary
        }
        // TODO: updates to secondaries

      } else if (command->variant == "Shutdown") {
        polling_thread.interrupt();
        return;
      }

    } catch (Uptane::Exception e) {
      LOGGER_LOG(LVL_error, e.what());
    } catch (const std::exception &ex) {
      LOGGER_LOG(LVL_error, "Unknown exception was thrown: " << ex.what());
    }
  }
}
