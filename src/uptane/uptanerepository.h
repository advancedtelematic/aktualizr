#ifndef UPTANE_REPOSITORY_H_
#define UPTANE_REPOSITORY_H_

#include <json/json.h>
#include <libp11.h>
#include <vector>
#include "config.h"
#include "invstorage.h"
#include "uptane/secondary.h"
#include "uptane/testbusprimary.h"
#include "uptane/tufrepository.h"

#include "crypto.h"
#include "httpinterface.h"
#include "logger.h"

class P11ContextWrapper {
 public:
  P11ContextWrapper(const std::string &module) {
    // never returns NULL
    ctx = PKCS11_CTX_new();
    if (PKCS11_CTX_load(ctx, module.c_str())) {
      PKCS11_CTX_free(ctx);
      LOGGER_LOG(LVL_error, "Couldn't load PKCS11 module " << module);
      throw std::runtime_error("PKCS11 error");
    }
  }
  ~P11ContextWrapper() {
    PKCS11_CTX_unload(ctx);
    PKCS11_CTX_free(ctx);
  }
  PKCS11_CTX *get() { return ctx; }

 private:
  PKCS11_CTX *ctx;
};

class P11SlotsWrapper {
 public:
  P11SlotsWrapper(PKCS11_CTX *ctx_in) {
    ctx = ctx_in;
    if (PKCS11_enumerate_slots(ctx, &slots, &nslots)) {
      LOGGER_LOG(LVL_error, "Couldn't enumerate slots");
      throw std::runtime_error("PKCS11 error");
    }
  }
  ~P11SlotsWrapper() { PKCS11_release_all_slots(ctx, slots, nslots); }
  PKCS11_SLOT *get_slots() { return slots; }
  unsigned int get_nslots() { return nslots; }

 private:
  PKCS11_CTX *ctx;
  PKCS11_SLOT *slots;
  unsigned int nslots;
};

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

  // implemented in uptane/initialize.cc
  bool initialize();
  void initReset();
  // TODO: only used by tests, rewrite test and delete this method
  void updateRoot(Version version = Version());
  const std::string &getPkcs11Keyname() const { return pkcs11_keyname; }
  const std::string &getPkcs11Certname() const { return pkcs11_certname; }

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
  Json::Value manifests;

  std::string primary_ecu_serial;
  std::string primary_public_key;
  std::string primary_private_key;

  std::string pkcs11_keyname;
  std::string pkcs11_certname;
  std::vector<Secondary> secondaries;
  TestBusPrimary transport;
  friend class TestBusSecondary;
  friend class ::SotaUptaneClient;
  bool verifyMeta(const Uptane::MetaPack &meta);
  bool getMeta();

  // implemented in uptane/initialize.cc
  bool initDeviceId(const ProvisionConfig &provision_config, const UptaneConfig &uptane_config);
  void resetDeviceId();
  bool initEcuSerials(UptaneConfig &uptane_config);
  void resetEcuSerials();
  bool initEcuKeys();
  void resetEcuKeys();
  InitRetCode initTlsCreds(const ProvisionConfig &provision_config, const TlsConfig &tls_config);
  void resetTlsCreds();
  InitRetCode initEcuRegister();
  void setEcuSerialsMembers(const std::vector<std::pair<std::string, std::string> > &ecu_serials);
  void setEcuKeysMembers(const std::string &primary_public, const std::string &primary_private);
};
}

#endif
