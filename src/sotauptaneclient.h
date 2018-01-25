#include <map>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "httpclient.h"
#include "invstorage.h"
#include "packagemanagerinterface.h"
#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"
#include "uptane/uptanerepository.h"

class SotaUptaneClient {
 public:
  SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in, Uptane::Repository &repo,
                   const boost::shared_ptr<INvStorage> storage_in, HttpInterface &http_client);
  void runForever(command::Channel *commands_channel);
  Json::Value AssembleManifest();

  // ecu_serial => secondary*
  std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> > secondaries;

 private:
  bool isInstalled(const Uptane::Target &target);
  std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets, const std::string &ecu_id);
  data::InstallOutcome PackageInstall(const Uptane::Target &package);
  void PackageInstallSetResult(const Uptane::Target &package);
  void reportHwInfo();
  void reportInstalledPackages();
  void run(command::Channel *commands_channel);
  void initSecondaries();
  void verifySecondaries();
  void sendMetadataToEcus(std::vector<Uptane::Target> targets);
  void sendImagesToEcus(std::vector<Uptane::Target> targets);
  const Config &config;
  event::Channel *events_channel;
  Uptane::Repository &uptane_repo;
  const boost::shared_ptr<INvStorage> storage;
  boost::shared_ptr<PackageManagerInterface> pacman;
  HttpInterface &http;
  int last_targets_version;
  Json::Value operation_result;
};

class SerialCompare {
 public:
  SerialCompare(const std::string &target_in) : target(target_in) {}
  bool operator()(std::pair<std::string, std::string> &in) { return (in.first == target); }

 private:
  std::string target;
};
