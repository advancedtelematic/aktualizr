#ifndef UPTANE_SECONDARY_H_
#define UPTANE_SECONDARY_H_

#include <boost/filesystem.hpp>
#include <string>

#include <json/json.h>
#include "uptane/testbussecondary.h"
#include "uptane/tufrepository.h"

namespace Uptane {
class Secondary {
 public:
  Secondary(const SecondaryConfig &config_in, Uptane::Repository *primary);
  void install(const Uptane::Target &target);
  Json::Value genAndSendManifest();
  void newTargetsCallBack(const std::vector<Target> &targets);

 private:
  SecondaryConfig config;
  TestBusSecondary transport;
};
};

#endif