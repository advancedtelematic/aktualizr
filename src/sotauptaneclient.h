#include <map>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "httpclient.h"
#include "ostree.h"
#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"
#include "uptane/uptanerepository.h"

class SotaUptaneClient {
 public:
  enum ServiceType { Director = 0, Repo };
  std::string getEndPointUrl(SotaUptaneClient::ServiceType, const std::string &endpoint);

  SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in, Uptane::Repository &repo);

  Json::Value sign(const Json::Value &in_data);
  Json::Value OstreeInstallAndManifest(const Uptane::Target &package);
  void run(command::Channel *commands_channel);
  void runForever(command::Channel *commands_channel);

 private:
  void reportHWInfo();
  void reportInstalledPackages();
  bool isInstalled(const Uptane::Target &target);
  data::InstallOutcome OstreeInstall(const Uptane::Target &package);
  OstreePackage uptaneToOstree(const Uptane::Target &target);
  std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets, std::string ecu_id);
  Config config;
  event::Channel *events_channel;
  Uptane::Repository &uptane_repo;

  std::map<std::string, std::vector<boost::shared_ptr<SecondaryInterface> > > secondaries;
  int last_targets_version;
};
