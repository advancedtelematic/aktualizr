#ifndef PRIMARY_MANAGEDSECONDARY_H_
#define PRIMARY_MANAGEDSECONDARY_H_

#include <future>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "primary/secondary_config.h"
#include "uptane/secondaryinterface.h"
#include "utilities/types.h"

namespace Primary {

class ManagedSecondaryConfig : public SecondaryConfig {
 public:
  ManagedSecondaryConfig(const char* type = Type) : SecondaryConfig(type) {}

 public:
  constexpr static const char* const Type = "managed";

 public:
  bool partial_verifying{false};
  std::string ecu_serial;
  std::string ecu_hardware_id;
  boost::filesystem::path full_client_dir;
  std::string ecu_private_key;
  std::string ecu_public_key;
  boost::filesystem::path firmware_path;
  boost::filesystem::path target_name_path;
  boost::filesystem::path metadata_path;
  KeyType key_type{KeyType::kRSA2048};
};

// Managed secondary is an abstraction over virtual and other types of legacy
// (non-Uptane) secondaries. They require all the Uptane-related functionality
// to be implemented in aktualizr itself, so there's some shared code.

class ManagedSecondary : public Uptane::SecondaryInterface {
 public:
  explicit ManagedSecondary(Primary::ManagedSecondaryConfig sconfig_in);
  ~ManagedSecondary() override = default;

  void Initialize();

  Uptane::EcuSerial getSerial() const override {
    if (!sconfig.ecu_serial.empty()) {
      return Uptane::EcuSerial(sconfig.ecu_serial);
    }
    return Uptane::EcuSerial(public_key_.KeyId());
  }
  Uptane::HardwareIdentifier getHwId() const override { return Uptane::HardwareIdentifier(sconfig.ecu_hardware_id); }
  PublicKey getPublicKey() const override { return public_key_; }
  bool putMetadata(const Uptane::RawMetaPack& meta_pack) override;
  int getRootVersion(bool director) const override;
  bool putRoot(const std::string& root, bool director) override;

  bool sendFirmware(const std::string& data) override;
  data::ResultCode::Numeric install(const std::string& target_name) override;

  Uptane::Manifest getManifest() const override;

  bool loadKeys(std::string* pub_key, std::string* priv_key);

 protected:
  Primary::ManagedSecondaryConfig sconfig;
  std::string detected_attack;
  std::string expected_target_name;
  std::vector<Hash> expected_target_hashes;
  uint64_t expected_target_length{};
  std::mutex install_mutex;

  virtual bool storeFirmware(const std::string& target_name, const std::string& content) = 0;
  virtual bool getFirmwareInfo(Uptane::InstalledImageInfo& firmware_info) const = 0;

 private:
  PublicKey public_key_;
  std::string private_key;
  Uptane::MetaPack current_meta;
  Uptane::RawMetaPack current_raw_meta;

  void storeKeys(const std::string& pub_key, const std::string& priv_key);
  void rawToMeta();

  // TODO: implement
  void storeMetadata(const Uptane::RawMetaPack& meta_pack) { (void)meta_pack; }
  bool loadMetadata(Uptane::RawMetaPack* meta_pack);
};

}  // namespace Primary

#endif  // PRIMARY_MANAGEDSECONDARY_H_
