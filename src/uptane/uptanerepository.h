#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include <json/json.h>
#include "config.h"

namespace Uptane {
class Repository {
 public:
  Repository(const Config& config);
  Json::Value sign(const Json::Value& in_data);
  std::string signManifest();
  std::string signManifest(const Json::Value&);

 private:
  Config config;
};
};

#endif