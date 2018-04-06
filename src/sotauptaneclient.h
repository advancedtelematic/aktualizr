#include <atomic>
#include <map>
#include <string>
#include <vector>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "httpclient.h"
#include "invstorage.h"
#include "ipsecondarydiscovery.h"
#include "ipuptaneconnection.h"
#include "ipuptaneconnectionsplitter.h"
#include "packagemanagerinterface.h"
#include "uptane/secondaryinterface.h"
#include "uptane/uptanerepository.h"

class SotaUptaneClient {
 public:
  SotaUptaneClient(Config &config_in, event::Channel *events_channel_in, Uptane::Repository &repo,
                   const std::shared_ptr<INvStorage> storage_in, HttpInterface &http_client);
  void runForever(command::Channel *commands_channel);
  Json::Value AssembleManifest();
  std::string secondaryTreehubCredentials() const;

  // ecu_serial => secondary*
  std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> > secondaries;

 private:
  bool isInstalled(const Uptane::Target &target);
  std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets, const std::string &ecu_id);
  data::InstallOutcome PackageInstall(const Uptane::Target &target);
  void PackageInstallSetResult(const Uptane::Target &target);
  void reportHwInfo();
  void reportInstalledPackages();
  void run(command::Channel *commands_channel);
  void initSecondaries();
  void verifySecondaries();
  void sendMetadataToEcus(std::vector<Uptane::Target> targets);
  void sendImagesToEcus(std::vector<Uptane::Target> targets);

  Config &config;
  event::Channel *events_channel;
  Uptane::Repository &uptane_repo;
  const std::shared_ptr<INvStorage> storage;
  std::shared_ptr<PackageManagerInterface> pacman;
  HttpInterface &http;
  int last_targets_version;
  Json::Value operation_result;
  std::unique_ptr<IpUptaneConnection> ip_uptane_connection;
  std::unique_ptr<IpUptaneConnectionSplitter> ip_uptane_splitter;
  std::atomic<bool> shutdown = {false};
};

class SerialCompare {
 public:
  SerialCompare(const std::string &target_in) : target(target_in) {}
  bool operator()(std::pair<std::string, std::string> &in) { return (in.first == target); }

 private:
  std::string target;
};
