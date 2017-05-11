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
  Json::Value getJSON(SotaUptaneClient::ServiceType service, const std::string &role);
  Json::Value sign(const Json::Value &in_data);
  void OstreeInstall(std::vector<OstreePackage> packages);
  std::vector<OstreePackage> getAvailableUpdates();
  bool deviceRegister();
  bool ecuRegister();
  bool authenticate();
  void run(command::Channel *commands_channel);
  void runForever(command::Channel *commands_channel);

 private:
  HttpClient *http;
  Config config;
  event::Channel *events_channel;
  Uptane::TufRepository director;
  Uptane::Repository uptane_repo;

  std::vector<Json::Value> ecu_versions;
  bool processing;
  bool was_error;
};
