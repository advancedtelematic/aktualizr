#ifndef INITIALIZER_H_
#define INITIALIZER_H_

#include "config/config.h"
#include "crypto/keymanager.h"
#include "http/httpinterface.h"
#include "uptane/secondaryinterface.h"

const int MaxInitializationAttempts = 3;

enum class InitRetCode { kOk, kOccupied, kServerFailure, kStorageFailure, kSecondaryFailure, kBadP12, kPkcs11Failure };

class Initializer {
 public:
  Initializer(const ProvisionConfig& config_in, std::shared_ptr<INvStorage> storage_in,
              std::shared_ptr<HttpInterface> http_client_in, KeyManager& keys_in,
              const std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface> >& secondary_info_in);
  bool isSuccessful() const { return success_; }

 private:
  const ProvisionConfig& config_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<HttpInterface> http_client_;
  KeyManager& keys_;
  const std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface> >& secondary_info_;
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
  bool loadSetTlsCreds();
  bool initEcuReportCounter();
};

#endif  // INITIALIZER_H_
