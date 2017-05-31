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
  void OstreeInstall(std::vector<OstreePackage> packages);
  std::vector<OstreePackage> getAvailableUpdates();
  void run(command::Channel *commands_channel);
  void runForever(command::Channel *commands_channel);

 private:
  Config config;
  event::Channel *events_channel;
  Uptane::Repository uptane_repo;

  std::vector<Json::Value> ecu_versions;
};
