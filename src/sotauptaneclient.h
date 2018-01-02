#include <map>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "httpclient.h"
#include "invstorage.h"
#include "ostree.h"
#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"
#include "uptane/uptanerepository.h"

class SotaUptaneClient {
 public:
  SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in, Uptane::Repository &repo,
                   const boost::shared_ptr<INvStorage> &storage_in, HttpInterface &http_client);
  void OstreeInstallSetResult(const Uptane::Target &package);
  void runForever(command::Channel *commands_channel);

  Json::Value AssembleManifest();

  // ecu_serial => secondary*
  std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> > secondaries;

 private:
  bool isInstalled(const Uptane::Target &target);
  std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets, const std::string &ecu_id);
  data::InstallOutcome OstreeInstall(const Uptane::Target &package);
  void reportHwInfo();
  void reportInstalledPackages();
  void run(command::Channel *commands_channel);
  OstreePackage uptaneToOstree(const Uptane::Target &target);
  void initSecondaries();
  void verifySecondaries();
  void updateSecondaries(std::vector<Uptane::Target> targets);

  const Config &config;
  event::Channel *events_channel;
  Uptane::Repository &uptane_repo;
  const boost::shared_ptr<INvStorage> &storage;
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
