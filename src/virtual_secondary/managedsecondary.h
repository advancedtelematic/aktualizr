#ifndef PRIMARY_MANAGEDSECONDARY_H_
#define PRIMARY_MANAGEDSECONDARY_H_

#include <future>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "libaktualizr/secondaryinterface.h"
#include "libaktualizr/types.h"
#include "primary/secondary_config.h"

namespace Primary {

struct MetaPack;

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

// ManagedSecondary is an abstraction over virtual and other types of legacy
// (non-Uptane) Secondaries. They require any the Uptane-related functionality
// to be implemented in aktualizr itself.
class ManagedSecondary : public SecondaryInterface {
 public:
  explicit ManagedSecondary(Primary::ManagedSecondaryConfig sconfig_in);
  ~ManagedSecondary() override;

  void init(std::shared_ptr<SecondaryProvider> secondary_provider_in) override {
    secondary_provider_ = std::move(secondary_provider_in);
  }
  void Initialize();

  Uptane::EcuSerial getSerial() const override {
    if (!sconfig.ecu_serial.empty()) {
      return Uptane::EcuSerial(sconfig.ecu_serial);
    }
    return Uptane::EcuSerial(public_key_.KeyId());
  }
  Uptane::HardwareIdentifier getHwId() const override { return Uptane::HardwareIdentifier(sconfig.ecu_hardware_id); }
  PublicKey getPublicKey() const override { return public_key_; }
  data::InstallationResult putMetadata(const Uptane::Target& target) override;
  int getRootVersion(bool director) const override;
  data::InstallationResult putRoot(const std::string& root, bool director) override;

  data::InstallationResult sendFirmware(const Uptane::Target& target) override;
  data::InstallationResult install(const Uptane::Target& target) override;

  Uptane::Manifest getManifest() const override;

  bool loadKeys(std::string* pub_key, std::string* priv_key);

 protected:
  virtual bool getFirmwareInfo(Uptane::InstalledImageInfo& firmware_info) const;

  std::shared_ptr<SecondaryProvider> secondary_provider_;
  Primary::ManagedSecondaryConfig sconfig;
  std::string detected_attack;

 private:
  void storeKeys(const std::string& pub_key, const std::string& priv_key);
  void rawToMeta();

  // TODO: implement persistent storage.
  bool storeMetadata() { return true; }
  bool loadMetadata() { return true; }

  PublicKey public_key_;
  std::string private_key;
  std::unique_ptr<MetaPack> current_meta;
  std::unique_ptr<Uptane::MetaBundle> meta_bundle_;
};

}  // namespace Primary

#endif  // PRIMARY_MANAGEDSECONDARY_H_
