#include "config/config.h"
#include "http/httpinterface.h"
#include "uptane/secondaryinterface.h"

const int MaxInitializationAttempts = 3;

enum InitRetCode {
  INIT_RET_OK,
  INIT_RET_OCCUPIED,
  INIT_RET_SERVER_FAILURE,
  INIT_RET_STORAGE_FAILURE,
  INIT_RET_SECONDARY_FAILURE,
  INIT_RET_BAD_P12,
  INIT_RET_PKCS11_FAILURE
};

class Initializer {
 public:
  Initializer(const ProvisionConfig& config, const std::shared_ptr<INvStorage>& storage, HttpInterface& http_client,
              KeyManager& keys,
              const std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >& secondary_info);
  bool isSuccessful() const { return success; }

 private:
  const ProvisionConfig& config;
  std::shared_ptr<INvStorage> storage;
  HttpInterface& http_client;
  KeyManager& keys;
  const std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >& secondary_info;
  bool success;

  bool initDeviceId();
  void resetDeviceId();
  bool initEcuSerials();
  void resetEcuSerials();
  bool initPrimaryEcuKeys();
  bool initSecondaryEcuKeys();
  void resetEcuKeys();
  InitRetCode initTlsCreds();
  void resetTlsCreds();
  InitRetCode initEcuRegister();
  bool loadSetTlsCreds();  // TODO -> metadownloader
};
