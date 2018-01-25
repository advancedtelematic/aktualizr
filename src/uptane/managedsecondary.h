#ifndef UPTANE_MANAGEDSECONDARY_H_
#define UPTANE_MANAGEDSECONDARY_H_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "types.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "uptane/tufrepository.h"

namespace Uptane {

// Managed secondary is an abstraction over virtual and legacy secondaries. Both of them have all the UPTANE-related
// functionality implemented in aktualizr itself, so there's some shared code.

class ManagedSecondary : public SecondaryInterface {
 public:
  ManagedSecondary(const SecondaryConfig& sconfig_in);

  virtual std::string getSerial() {
    if (!sconfig.ecu_serial.empty()) {
      return sconfig.ecu_serial;
    } else {
      return public_key_id;
    }
  }
  virtual std::string getPublicKey() { return public_key; }
  virtual bool putMetadata(const MetaPack& meta_pack);
  virtual int getRootVersion(const bool director);
  virtual bool putRoot(Uptane::Root root, const bool director);

  virtual bool sendFirmware(const std::string& data);
  virtual Json::Value getManifest();

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
}

#endif  // UPTANE_MANAGEDSECONDARY_H_
