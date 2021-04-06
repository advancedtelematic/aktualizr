#ifndef INITIALIZER_H_
#define INITIALIZER_H_

#include "libaktualizr/secondaryinterface.h"

#include "crypto/keymanager.h"
#include "http/httpinterface.h"
#include "libaktualizr/config.h"
#include "storage/invstorage.h"
#include "uptane/tuf.h"

const int MaxInitializationAttempts = 3;

class Initializer {
 public:
  Initializer(const ProvisionConfig& config_in, std::shared_ptr<INvStorage> storage_in,
              std::shared_ptr<HttpInterface> http_client_in, KeyManager& keys_in,
              const std::map<Uptane::EcuSerial, std::shared_ptr<SecondaryInterface> >& secondaries_in);

  class Error : public std::runtime_error {
   public:
    explicit Error(const std::string& what) : std::runtime_error(std::string("Initializer error: ") + what) {}
  };
  class KeyGenerationError : public Error {
   public:
    explicit KeyGenerationError(const std::string& what)
        : Error(std::string("Could not generate Uptane key pair: ") + what) {}
  };
  class StorageError : public Error {
   public:
    explicit StorageError(const std::string& what) : Error(std::string("Storage error: ") + what) {}
  };
  class ServerError : public Error {
   public:
    explicit ServerError(const std::string& what) : Error(std::string("Server error: ") + what) {}
  };
  class ServerOccupied : public Error {
   public:
    ServerOccupied() : Error("") {}
  };

 private:
  void initDeviceId();
  void resetDeviceId();
  void initEcuSerials();
  void initPrimaryEcuKeys();
  void initTlsCreds();
  void resetTlsCreds();
  void initSecondaryInfo();
  void initEcuRegister();
  bool loadSetTlsCreds();
  void initEcuReportCounter();

  const ProvisionConfig& config_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<HttpInterface> http_client_;
  KeyManager& keys_;
  const std::map<Uptane::EcuSerial, SecondaryInterface::Ptr>& secondaries_;
  std::vector<SecondaryInfo> sec_info_;
  EcuSerials new_ecu_serials_;
  bool register_ecus_{false};
};

class EcuCompare {
 public:
  explicit EcuCompare(std::pair<Uptane::EcuSerial, Uptane::HardwareIdentifier> ecu_in)
      : serial(std::move(ecu_in.first)), hardware_id(std::move(ecu_in.second)) {}
  bool operator()(const std::pair<Uptane::EcuSerial, Uptane::HardwareIdentifier>& in) const {
    return (in.first == serial && in.second == hardware_id);
  }

 private:
  const Uptane::EcuSerial serial;
  const Uptane::HardwareIdentifier hardware_id;
};

#endif  // INITIALIZER_H_
