#ifndef UPTANE_MANAGEDSECONDARY_H_
#define UPTANE_MANAGEDSECONDARY_H_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "utilities/types.h"

namespace Uptane {

// Managed secondary is an abstraction over virtual and legacy secondaries. Both of them have all the UPTANE-related
// functionality implemented in aktualizr itself, so there's some shared code.

class ManagedSecondary : public SecondaryInterface {
 public:
  ManagedSecondary(const SecondaryConfig& sconfig_in);

  std::string getSerial() override {
    if (!sconfig.ecu_serial.empty()) {
      return sconfig.ecu_serial;
    } else {
      return public_key_id;
    }
  }
  std::pair<KeyType, std::string> getPublicKey() override { return std::make_pair(sconfig.key_type, public_key); }
  bool putMetadata(const MetaPack& meta_pack) override;
  int getRootVersion(const bool director) override;
  bool putRoot(Uptane::Root root, const bool director) override;

  bool sendFirmware(const std::string& data) override;
  Json::Value getManifest() override;

  bool loadKeys(std::string* public_key, std::string* private_key);

 private:
  std::string public_key;
  std::string private_key;
  std::string public_key_id;

  std::string detected_attack;
  std::string expected_target_name;
  std::vector<Hash> expected_target_hashes;
  int64_t expected_target_length;

  MetaPack current_meta;

  virtual bool storeFirmware(const std::string& target_name, const std::string& content) = 0;
  virtual bool getFirmwareInfo(std::string* target_name, size_t& target_len, std::string* sha256hash) = 0;

  void storeKeys(const std::string& public_key, const std::string& private_key);

  // TODO: implement
  void storeMetadata(const MetaPack& meta_pack) { (void)meta_pack; }
  bool loadMetadata(MetaPack* meta_pack);
};
}  // namespace Uptane

#endif  // UPTANE_MANAGEDSECONDARY_H_
