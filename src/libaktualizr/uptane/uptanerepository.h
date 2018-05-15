#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include <vector>

#include "json/json.h"

#include "config/config.h"
#include "crypto/crypto.h"
#include "crypto/keymanager.h"
#include "logging/logging.h"
#include "storage/invstorage.h"

namespace Uptane {

class Repository {
 public:
  Repository(const Config &config_in, std::shared_ptr<INvStorage> storage_in);
  Json::Value signManifest(const Json::Value &version_manifests);
  Json::Value signVersionManifest(const Json::Value &primary_version_manifests);
  std::pair<int, std::vector<Uptane::Target> > getTargets();
  std::string getPrimaryEcuSerial() const { return primary_ecu_serial; };
  void setPrimaryEcuSerialHwId(const std::pair<std::string, Uptane::HardwareIdentifier> &serials) {
    primary_ecu_serial = serials.first;
    primary_hardware_id = serials.second;
  }

  bool feedCheckMeta();
  bool feedCheckRoot(bool director, Version version);

  // TODO: Receive and update time nonces.

  Uptane::MetaPack &currentMeta() { return meta_; }
  bool verifyMetaTargets(const Uptane::Targets &director_targets, const Uptane::Targets &image_targets);

  const Exception &getLastException() { return last_exception; }  // used by test_uptane_vectors

 private:
  const Config &config;
  std::shared_ptr<INvStorage> storage;

  KeyManager keys_;

  Json::Value manifests;
  MetaPack meta_;
  Exception last_exception{"", ""};

  std::string primary_ecu_serial;

  Uptane::HardwareIdentifier primary_hardware_id{Uptane::HardwareIdentifier::Unknown()};
};
}  // namespace Uptane

#endif
