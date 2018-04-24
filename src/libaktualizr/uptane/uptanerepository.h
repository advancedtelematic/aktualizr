#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include <vector>

#include "json/json.h"

#include "config.h"
#include "httpinterface.h"
#include "logging/logging.h"
#include "storage/invstorage.h"
#include "uptane/secondaryinterface.h"
#include "utilities/crypto.h"
#include "utilities/keymanager.h"

namespace Uptane {

enum InitRetCode {
  INIT_RET_OK,
  INIT_RET_OCCUPIED,
  INIT_RET_SERVER_FAILURE,
  INIT_RET_STORAGE_FAILURE,
  INIT_RET_SECONDARY_FAILURE,
  INIT_RET_BAD_P12,
  INIT_RET_PKCS11_FAILURE
};

struct DownloadMetaStruct {
  int64_t expected_length{};
  int64_t downloaded_length{};
  StorageTargetWHandle *fhandle{};
  MultiPartSHA256Hasher sha256_hasher;
  MultiPartSHA512Hasher sha512_hasher;
};

const int MaxInitializationAttempts = 3;

class Repository {
 public:
  Repository(const Config &config_in, std::shared_ptr<INvStorage> storage_in, HttpInterface &http_client);
  bool putManifest(const Json::Value &version_manifests);
  Json::Value signVersionManifest(const Json::Value &primary_version_manifests);
  void addSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &sec) { secondary_info.push_back(sec); }
  std::pair<int, std::vector<Uptane::Target> > getTargets();
  std::string getPrimaryEcuSerial() const { return primary_ecu_serial; };

  bool getMeta();

  // implemented in uptane/initialize.cc
  bool initialize();
  void initReset();
  // TODO: only used by tests, rewrite test and delete this method
  void updateRoot(Version version = Version());
  // TODO: Receive and update time nonces.

  Uptane::MetaPack &currentMeta() { return meta_; }
  bool verifyMetaTargets(const Uptane::Targets &director_targets, const Uptane::Targets &image_targets);

 private:
  const Config &config;
  std::shared_ptr<INvStorage> storage;
  HttpInterface &http;

  KeyManager keys_;

  Json::Value manifests;
  MetaPack meta_;

  std::string primary_ecu_serial;
  std::string primary_hardware_id_;

  std::vector<std::shared_ptr<Uptane::SecondaryInterface> > secondary_info;

  void downloadTarget(const Target &target);
  Json::Value getJSON(const std::string &url);
  Json::Value fetchRole(const std::string &base_url, Uptane::Role role, Version version = Version());

  // implemented in uptane/initialize.cc
  bool initDeviceId(const ProvisionConfig &provision_config, const UptaneConfig &uptane_config);
  void resetDeviceId();
  bool initEcuSerials(const UptaneConfig &uptane_config);
  void resetEcuSerials();
  bool initPrimaryEcuKeys();
  bool initSecondaryEcuKeys();

  void resetEcuKeys();
  InitRetCode initTlsCreds(const ProvisionConfig &provision_config);
  void resetTlsCreds();
  InitRetCode initEcuRegister();
  bool loadSetTlsCreds();
  void setEcuSerialMembers(const std::pair<std::string, std::string> &ecu_serials);
};
}  // namespace Uptane

#endif
