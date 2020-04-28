#ifndef INITIALIZER_H_
#define INITIALIZER_H_

#include "config/config.h"
#include "crypto/keymanager.h"
#include "http/httpinterface.h"
#include "uptane/secondaryinterface.h"

const int MaxInitializationAttempts = 3;

class Initializer {
 public:
  Initializer(const ProvisionConfig& config_in, std::shared_ptr<INvStorage> storage_in,
              std::shared_ptr<HttpInterface> http_client_in, KeyManager& keys_in,
              const std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface> >& secondaries_in);

  class Error : public std::runtime_error {
   public:
    Error(const std::string& what) : std::runtime_error(std::string("Initializer error: ") + what) {}
  };
  class KeyGenerationError : public Error {
   public:
    KeyGenerationError(const std::string& what) : Error(std::string("Could not generate uptane key pair: ") + what) {}
  };
  class StorageError : public Error {
   public:
    StorageError(const std::string& what) : Error(std::string("Storage error: ") + what) {}
  };
  class ServerError : public Error {
   public:
    ServerError(const std::string& what) : Error(std::string("Server error: ") + what) {}
  };
  class ServerOccupied : public Error {
   public:
    ServerOccupied() : Error("") {}
  };

 private:
  const ProvisionConfig& config_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<HttpInterface> http_client_;
  KeyManager& keys_;
  const std::map<Uptane::EcuSerial, Uptane::SecondaryInterface::Ptr>& secondaries_;

  void initDeviceId();
  void resetDeviceId();
  void initEcuSerials();
  void resetEcuSerials();
  void initPrimaryEcuKeys();
  void resetEcuKeys();
  void initTlsCreds();
  void resetTlsCreds();
  void initEcuRegister();
  bool loadSetTlsCreds();
  void initEcuReportCounter();
};

#endif  // INITIALIZER_H_
