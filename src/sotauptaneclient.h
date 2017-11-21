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
  SotaUptaneClient(const Config &config_in, event::Channel *events_channel_in, Uptane::Repository &repo);
  void run(command::Channel *commands_channel);
  Json::Value OstreeInstallAndManifest(const Uptane::Target &package);
  void runForever(command::Channel *commands_channel);

 private:
  bool isInstalled(const Uptane::Target &target);
  std::vector<Uptane::Target> findForEcu(const std::vector<Uptane::Target> &targets, std::string ecu_id);
  data::InstallOutcome OstreeInstall(const Uptane::Target &package);
  void reportHwInfo();
  void reportInstalledPackages();
  OstreePackage uptaneToOstree(const Uptane::Target &target);

  Config config;
  event::Channel *events_channel;
  Uptane::Repository &uptane_repo;
  std::map<std::string, std::vector<boost::shared_ptr<SecondaryInterface> > > secondaries;
  int last_targets_version;
};
