#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include <json/json.h>
#include <vector>
#include "config.h"
#include "invstorage.h"
#include "uptane/secondary.h"
#include "uptane/testbusprimary.h"
#include "uptane/tufrepository.h"

#include "crypto.h"
#include "httpinterface.h"
#include "logger.h"

#ifdef BUILD_P11
#include "p11engine.h"
#endif

class SotaUptaneClient;

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
const int MaxInitializationAttempts = 3;
class Repository {
 public:
  Repository(const Config &config, INvStorage &storage, HttpInterface &http_client);
  bool putManifest(const Json::Value &version_manifests);
  Json::Value getCurrentVersionManifests(const Json::Value &version_manifests);
  // void addSecondary(const std::string &ecu_serial, const std::string &hardware_identifier);
  Json::Value updateSecondaries(const std::vector<Uptane::Target> &secondary_targets);
  std::pair<int, std::vector<Uptane::Target> > getTargets();
  std::string getPrimaryEcuSerial() const { return primary_ecu_serial; };
  std::string getInstalledRefName() const;
  void saveInstalledVersion(const Target &target);
  std::string findInstalledVersion(const std::string &hash);

  // implemented in uptane/initialize.cc
  bool initialize();
  void initReset();
  // TODO: only used by tests, rewrite test and delete this method
  void updateRoot(Version version = Version());

 private:
  struct SecondaryConfig {
    std::string ecu_serial;
    std::string ecu_hardware_id;
  };
  Config config;
  TufRepository director;
  TufRepository image;
  INvStorage &storage;
  HttpInterface &http;
#ifdef BUILD_P11
  P11Engine p11;
#endif
  Json::Value manifests;

  std::string primary_ecu_serial;

  CryptoSource key_source;
  std::string primary_public_key;
  std::string primary_private_key;
  std::string primary_public_key_id;

  std::string pkcs11_tls_keyname;
  std::string pkcs11_tls_certname;
  std::vector<Secondary> secondaries;
  TestBusPrimary transport;
  friend class TestBusSecondary;
  friend class ::SotaUptaneClient;
  bool verifyMeta(const Uptane::MetaPack &meta);
  bool getMeta();

  // implemented in uptane/initialize.cc
  bool initDeviceId(const ProvisionConfig &provision_config, const UptaneConfig &uptane_config,
                    const TlsConfig &tls_config);
  void resetDeviceId();
  bool initEcuSerials(UptaneConfig &uptane_config);
  void resetEcuSerials();
  bool initEcuKeys(const UptaneConfig &uptane_config);
  void resetEcuKeys();
  InitRetCode initTlsCreds(const ProvisionConfig &provision_config, const TlsConfig &tls_config);
  void resetTlsCreds();
  InitRetCode initEcuRegister(const UptaneConfig &uptane_config);
  bool loadSetTlsCreds(const TlsConfig &tls_config);
  void setEcuSerialsMembers(const std::vector<std::pair<std::string, std::string> > &ecu_serials);
  void setEcuKeysMembers(const std::string &primary_public, const std::string &primary_private,
                         const std::string &primary_public_id, CryptoSource source);
};
}

#endif
