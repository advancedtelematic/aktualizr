#ifndef UPTANE_SECONDARY_H_
#define UPTANE_SECONDARY_H_

#include <string>
#include "types.h"

#include <json/json.h>
#include <boost/filesystem.hpp>

#include "uptane/secondaryconfig.h"
#include "uptane/testbussecondary.h"
#include "uptane/tufrepository.h"

namespace Uptane {
class VirtualSecondary {
 public:
  VirtualSecondary(const SecondaryConfig &config_in, Uptane::Repository *primary);
  void setKeys(const std::string &public_key, const std::string &private_key);
  std::string getEcuSerial() { return config.ecu_serial; }
  bool getPublicKey(std::string *keytype, std::string *key);
  Json::Value verifyMeta(const TimeMeta& time_meta, const Root& root_meta, const Targets& targets_meta);
  bool writeImage(const uint8_t* blob, size_t size);
  Json::Value genAndSendManifest(Json::Value custom = Json::Value(Json::nullValue));

 private:
  SecondaryConfig config;

  std::string detected_attack;
  bool wait_for_target;
  std::string expected_target_name;
  std::vector<Hash> expected_target_hashes;
  int64_t expected_target_length;

};
}

#endif
