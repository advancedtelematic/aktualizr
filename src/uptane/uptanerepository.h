#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include <json/json.h>
#include <vector>
#include "config.h"
#include "invstorage.h"
#include "uptane/secondary.h"
#include "uptane/testbusprimary.h"
#include "uptane/tufrepository.h"

#include "crypto.h"

class SotaUptaneClient;

namespace Uptane {

class Repository {
 public:
  Repository(const Config &config, INvStorage &storage);
  bool putManifest();
  bool putManifest(const Json::Value &);
  void addSecondary(const std::string &ecu_serial, const std::string &hardware_identifier);

  // pair of (Version, targets[])
  std::pair<int, std::vector<Uptane::Target> > getTargets();
  bool deviceRegister();
  bool ecuRegister();
  // TODO: only used by tests, rewrite test and delete this method
  void updateRoot(Version version = Version());

 private:
  struct SecondaryConfig {
    std::string ecu_serial;
    std::string ecu_hardware_id;
  };
  Config config;
  TufRepository director;
  TufRepository image;
  INvStorage &storage;
  HttpClient http;
  Json::Value manifests;
  std::vector<SecondaryConfig> registered_secondaries;

  std::vector<Secondary> secondaries;
  TestBusPrimary transport;
  friend class TestBusSecondary;
  friend class ::SotaUptaneClient;
  bool verifyMeta(const Uptane::MetaPack &meta);
  bool getMeta();
};
}

#endif
