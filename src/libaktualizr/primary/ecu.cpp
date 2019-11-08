#include "ecu.h"

#include "package_manager/packagemanagerfactory.h"
#include "http/httpclient.h"

Ecu::Ecu() {

}

Ecu::~Ecu() {

}


Json::Value BaseEcu::getManifest() const {
  Uptane::Target installed_target = packman()->getCurrent();
  Json::Value installed_image;
  installed_image["filepath"] = installed_target.filename();
  installed_image["fileinfo"]["length"] = Json::UInt64(installed_target.length());
  installed_image["fileinfo"]["hashes"]["sha256"] = installed_target.sha256Hash();

  Json::Value unsigned_ecu_version;
  unsigned_ecu_version["attacks_detected"] = "";
  unsigned_ecu_version["installed_image"] = installed_image;
  unsigned_ecu_version["ecu_serial"] = serial().ToString();
  unsigned_ecu_version["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
  unsigned_ecu_version["timeserver_time"] = "1970-01-01T00:00:00Z";
  return unsigned_ecu_version;
}

PrimaryEcu::PrimaryEcu(const Config&               config,
                       std::shared_ptr<INvStorage>& storage,
                       const std::shared_ptr<Bootloader>& bootloader,
                       const std::shared_ptr<HttpInterface>&  http_client
                       ): BaseEcu(config.provision.primary_ecu_serial), _storage(storage) {

  _pacman = PackageManagerFactory::makePackageManager(config.pacman, _storage, bootloader, http_client);
}
