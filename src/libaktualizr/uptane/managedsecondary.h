#ifndef UPTANE_MANAGEDSECONDARY_H_
#define UPTANE_MANAGEDSECONDARY_H_

#include <future>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "utilities/types.h"

namespace Uptane {

// Managed secondary is an abstraction over virtual and other types of legacy
// (non-UPTANE) secondaries. They require all the UPTANE-related functionality
// to be implemented in aktualizr itself, so there's some shared code.

class ManagedSecondary : public SecondaryInterface {
 public:
  explicit ManagedSecondary(const SecondaryConfig& sconfig_in);
  ~ManagedSecondary() override = default;

  void Initialize() override;

  EcuSerial getSerial() override {
    if (!sconfig.ecu_serial.empty()) {
      return EcuSerial(sconfig.ecu_serial);
    }
    return EcuSerial(public_key_.KeyId());
  }
  PublicKey getPublicKey() override { return public_key_; }
  bool putMetadata(const RawMetaPack& meta_pack) override;
  int getRootVersion(bool director) override;
  bool putRoot(const std::string& root, bool director) override;

  bool sendFirmware(const std::shared_ptr<std::string>& data) override;
  Json::Value getManifest() override;

  bool loadKeys(std::string* pub_key, std::string* priv_key);

 private:
  PublicKey public_key_;
  std::string private_key;

  std::string detected_attack;
  std::string expected_target_name;
  std::vector<Hash> expected_target_hashes;
  uint64_t expected_target_length{};

  MetaPack current_meta;
  RawMetaPack current_raw_meta;
  std::mutex install_mutex;

  virtual bool storeFirmware(const std::string& target_name, const std::string& content) = 0;
  virtual bool getFirmwareInfo(std::string* target_name, size_t& target_len, std::string* sha256hash) = 0;

  void storeKeys(const std::string& pub_key, const std::string& priv_key);
  void rawToMeta();

  // TODO: implement
  void storeMetadata(const RawMetaPack& meta_pack) { (void)meta_pack; }
  bool loadMetadata(RawMetaPack* meta_pack);
};
}  // namespace Uptane

#endif  // UPTANE_MANAGEDSECONDARY_H_
