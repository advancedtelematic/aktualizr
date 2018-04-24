#include <json/json.h>
#include <atomic>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "ipsecondarydiscovery.h"
#include "package_manager/packagemanagerinterface.h"
#include "secondary_ipc/ipuptaneconnection.h"
#include "secondary_ipc/ipuptaneconnectionsplitter.h"
#include "storage/invstorage.h"
#include "uptane/secondaryinterface.h"
#include "uptane/uptanerepository.h"
#include "utilities/httpclient.h"

class SotaUptaneClient {
 public:
  SotaUptaneClient(Config &config_in, std::shared_ptr<event::Channel> events_channel_in, Uptane::Repository &repo,
                   std::shared_ptr<INvStorage> storage_in, HttpInterface &http_client);
  void runForever(const std::shared_ptr<command::Channel> &commands_channel);
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
  void schedulePoll(const std::shared_ptr<command::Channel> &commands_channel);
  void reportNetworkInfo();
  void initSecondaries();
  void verifySecondaries();
  void sendMetadataToEcus(std::vector<Uptane::Target> targets);
  void sendImagesToEcus(std::vector<Uptane::Target> targets);
  bool hasPendingUpdates(const Json::Value &manifests);
  bool putManifest();

  Config &config;
  std::shared_ptr<event::Channel> events_channel;
  Uptane::Repository &uptane_repo;
  const std::shared_ptr<INvStorage> storage;
  std::shared_ptr<PackageManagerInterface> pacman;
  HttpInterface &http;
  int last_targets_version;
  Json::Value operation_result;
  std::unique_ptr<IpUptaneConnection> ip_uptane_connection;
  std::unique_ptr<IpUptaneConnectionSplitter> ip_uptane_splitter;
  std::atomic<bool> shutdown = {false};
  Json::Value last_network_info_reported;
};

class SerialCompare {
 public:
  explicit SerialCompare(std::string target_in) : target(std::move(target_in)) {}
  bool operator()(std::pair<std::string, std::string> &in) { return (in.first == target); }

 private:
  std::string target;
};
