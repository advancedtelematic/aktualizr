#ifndef UPTANE_PARTIALVRIFICATIONSECONDARY_H_
#define UPTANE_PARTIALVRIFICATIONSECONDARY_H_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "types.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"

namespace Uptane {

class PartialVerificationSecondary : public SecondaryInterface {
 public:
  PartialVerificationSecondary(const SecondaryConfig& sconfig_in);

  std::string getSerial() override {
    if (!sconfig.ecu_serial.empty()) {
      return sconfig.ecu_serial;
    } else {
      return public_key_id;
    }
  }
  std::string getPublicKey() override { return public_key; }
  bool putMetadata(const MetaPack& meta_pack) override;
  int getRootVersion(const bool director) override;
  bool putRoot(Uptane::Root root, const bool director) override;

  bool sendFirmware(const std::string& data) override;
  Json::Value getManifest() override;

 private:
  std::string public_key;
  std::string private_key;
  std::string public_key_id;

  std::string detected_attack_;
  Uptane::Targets meta_targets_;
};
}

#endif  // UPTANE_PARTIALVRIFICATIONSECONDARY_H_