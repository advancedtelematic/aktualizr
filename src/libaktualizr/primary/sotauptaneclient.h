#include <gtest/gtest_prod.h>
#include <json/json.h>
#include <atomic>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "commands.h"
#include "config/config.h"
#include "events.h"
#include "http/httpclient.h"
#include "package_manager/packagemanagerinterface.h"
#include "secondary_ipc/ipuptaneconnection.h"
#include "secondary_ipc/ipuptaneconnectionsplitter.h"
#include "storage/invstorage.h"
#include "uptane/ipsecondarydiscovery.h"
#include "uptane/secondaryinterface.h"
#include "uptane/uptanerepository.h"

class SotaUptaneClient {
 public:
  SotaUptaneClient(Config &config_in, std::shared_ptr<event::Channel> events_channel_in, Uptane::Repository &repo,
                   std::shared_ptr<INvStorage> storage_in, HttpInterface &http_client);

  void getUpdateRequests();
  void runForever(const std::shared_ptr<command::Channel> &commands_channel);
  Json::Value AssembleManifest();
  std::string secondaryTreehubCredentials() const;

  // ecu_serial => secondary*
  std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> > secondaries;

 private:
  FRIEND_TEST(UptaneKey, CheckAllKeys);
  FRIEND_TEST(UptaneKey, RecoverWithoutKeys);
  FRIEND_TEST(aktualizr_secondary_uptane, credentialsPassing);
  FRIEND_TEST(Uptane, RandomSerial);
  FRIEND_TEST(Uptane, ReloadSerial);
  FRIEND_TEST(Uptane, LegacySerial);
  FRIEND_TEST(Uptane, AssembleManifestGood);
  FRIEND_TEST(Uptane, AssembleManifestBad);
  FRIEND_TEST(Uptane, PutManifest);
  FRIEND_TEST(Uptane, UptaneSecondaryAdd);
  FRIEND_TEST(UptaneCI, OneCycleUpdate);
  FRIEND_TEST(UptaneCI, CheckKeys);

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
  bool initialize();

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
