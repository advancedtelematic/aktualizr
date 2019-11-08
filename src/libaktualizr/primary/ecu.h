#ifndef ECU_H_
#define ECU_H_

#include "uptane/tuf.h"
#include <unordered_map>

#include "config/config.h"
#include "storage/invstorage.h"
#include "bootloader/bootloader.h"
#include "http/httpclient.h"

class PackageManagerInterface;

class Ecu {
  public:
    using Ptr = std::shared_ptr<Ecu>;
  public:
    Ecu();
    virtual ~Ecu();

  public:
    virtual const Uptane::EcuSerial& serial() const = 0;

    virtual std::shared_ptr<PackageManagerInterface> packman() const = 0;

    //virtual Uptane::Target currentImageVersion() const = 0;

    virtual Json::Value getManifest() const = 0;

    //virtual bool imageUpdated() = 0;

    //virtual bool downloadTargetImage(const Uptane::Target& target) = 0;
};

class BaseEcu: public Ecu {
  public:
    BaseEcu(const std::string& serial):_serial(serial) {}

    virtual const Uptane::EcuSerial& serial() const {
      return _serial;
    }

    virtual Json::Value getManifest() const;

  private:
    Uptane::EcuSerial _serial;
};




class PrimaryEcu:public BaseEcu {
  public:
    PrimaryEcu(const Config& config,
               std::shared_ptr<INvStorage>& storage,
               const std::shared_ptr<Bootloader>& bootloader,
               const std::shared_ptr<HttpInterface>&  http_client);
  public:
    virtual std::shared_ptr<PackageManagerInterface> packman() const {
      return _pacman;
    }

  private:

    std::shared_ptr<INvStorage>& _storage;
    std::shared_ptr<PackageManagerInterface> _pacman;
};


class EcuRegistry {
  public:
    EcuRegistry(){}
    ~EcuRegistry(){}

  public:
    void addEcu(Ecu::Ptr ecu) {
      _ecus.emplace(ecu->serial(), ecu);
    }

    Ecu::Ptr operator[](const Uptane::EcuSerial& serial) {
      return _ecus.at(serial);
    }
  private:
    std::unordered_map<Uptane::EcuSerial, Ecu::Ptr> _ecus;
};

#endif // ECU_H_
