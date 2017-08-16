#ifndef UPTANE_SECONDARY_H_
#define UPTANE_SECONDARY_H_

#include <boost/filesystem.hpp>
#include <string>

#include <json/json.h>
#include "types.h"
#include "uptane/testbussecondary.h"
#include "uptane/tufrepository.h"

namespace Uptane {
class Secondary {
 public:
  Secondary(const SecondaryConfig &config_in, Uptane::Repository *primary);
  data::InstallOutcome install(const Uptane::Target &target);
  Json::Value genAndSendManifest(Json::Value custom = Json::Value(Json::nullValue));
  Json::Value newTargetsCallBack(const std::vector<Target> &targets);
  void setKeys(const std::string &public_key, const std::string &private_key);
  std::string getEcuSerial() { return config.ecu_serial; }
  bool getPublicKey(std::string *key);

 private:
  SecondaryConfig config;
  TestBusSecondary transport;
};
}

#endif
