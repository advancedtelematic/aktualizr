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
  void updateRoot();
  Json::Value sign(const Json::Value& in_data);
  void putManifest();
  void putManifest(const Json::Value&);
  std::vector<Uptane::Target> getNewTargets();
  bool deviceRegister();
  bool ecuRegister();
  bool authenticate();

 private:
  Config config;
  TufRepository director;
  TufRepository image;
  HttpClient http;
};
};

#endif