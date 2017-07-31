#include <map>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "httpclient.h"
#include "ostree.h"
#include "uptane/tufrepository.h"
#include "uptane/uptanerepository.h"

class SotaUptaneClient {
 public:
  enum ServiceType {
    Director = 0,
    Repo,
  };
  std::string getEndPointUrl(SotaUptaneClient::ServiceType, const std::string &endpoint);

  SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in);

  void putManifest(SotaUptaneClient::ServiceType service, const std::string &manifest);
  Json::Value sign(const Json::Value &in_data);
  void OstreeInstall(const OstreePackage &package);
  void run(command::Channel *commands_channel);
  void runForever(command::Channel *commands_channel);

 private:
  void reportHWInfo();
  void reportInstalledPackages();
  std::vector<Uptane::Target> getUpdates();
  bool isInstalled(const Uptane::Target &target);
  OstreePackage uptaneToOstree(const Uptane::Target &target);
  std::vector<Uptane::Target> findOstree(const std::vector<Uptane::Target> &targets);
  std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets, std::string ecu_id);
  Config config;
  event::Channel *events_channel;
  Uptane::Repository uptane_repo;

  std::vector<Json::Value> ecu_versions;
};
