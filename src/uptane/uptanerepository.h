#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include <json/json.h>
#include <vector>
#include "config.h"
#include "uptane/tufrepository.h"

namespace Uptane {

class Repository {
 public:
  Repository(const Config& config);
  Json::Value sign(const Json::Value& in_data);
  std::string signManifest();
  std::string signManifest(const Json::Value&);
  std::vector<Uptane::Target> getNewTargets();

 private:
  Config config;
  TufRepository director;
  TufRepository image;
};
};

#endif