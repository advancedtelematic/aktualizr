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
      return public_key_id_;
    }
  }
  std::string getPublicKey() override { return public_key_; }
  bool putMetadata(const MetaPack& meta) override;
  int getRootVersion(const bool director) override;
  bool putRoot(Uptane::Root root, const bool director) override;

  bool sendFirmware(const std::string& data) override;
  Json::Value getManifest() override;

 private:
  void storeKeys(const std::string& public_key, const std::string& private_key);
  bool loadKeys(std::string* public_key, std::string* private_key);

  Uptane::Root root_;
  std::string public_key_;
  std::string private_key_;
  std::string public_key_id_;

  std::string detected_attack_;
  Uptane::Targets meta_targets_;
};
}

#endif  // UPTANE_PARTIALVRIFICATIONSECONDARY_H_