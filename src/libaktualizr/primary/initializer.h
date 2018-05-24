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
  Initializer(const ProvisionConfig& config_in, std::shared_ptr<INvStorage> storage_in, HttpInterface& http_client_in,
              KeyManager& keys_in,
              const std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >& secondary_info_in);
  bool isSuccessful() const { return success_; }

 private:
  const ProvisionConfig& config_;
  std::shared_ptr<INvStorage> storage_;
  HttpInterface& http_client_;
  KeyManager& keys_;
  const std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >& secondary_info_;
  bool success_;

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
