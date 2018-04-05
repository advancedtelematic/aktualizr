#ifndef PACKAGEMANAGERINTERFACE_H_
#define PACKAGEMANAGERINTERFACE_H_

#include <string>

#include "types.h"
#include "uptane/tuf.h"

class PackageManagerInterface {
 public:
  virtual ~PackageManagerInterface() = default;
  virtual Json::Value getInstalledPackages() = 0;
  virtual Uptane::Target getCurrent() = 0;
  virtual data::InstallOutcome install(const Uptane::Target &target) const = 0;
  Uptane::Target getUnknown() {
    Json::Value t_json;
    t_json["hashes"]["sha256"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
    t_json["length"] = 0;
    return Uptane::Target("unknown", t_json);
  }
  Json::Value getManifest(const std::string &ecu_serial) {
    Uptane::Target installed_target = getCurrent();
    Json::Value installed_image;
    installed_image["filepath"] = installed_target.filename();
    installed_image["fileinfo"]["length"] = Json::UInt64(installed_target.length());
    installed_image["fileinfo"]["hashes"]["sha256"] = installed_target.sha256Hash();

    Json::Value unsigned_ecu_version;
    unsigned_ecu_version["attacks_detected"] = "";
    unsigned_ecu_version["installed_image"] = installed_image;
    unsigned_ecu_version["ecu_serial"] = ecu_serial;
    unsigned_ecu_version["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
    unsigned_ecu_version["timeserver_time"] = "1970-01-01T00:00:00Z";
    return unsigned_ecu_version;
  }
};

#endif  // PACKAGEMANAGERINTERFACE_H_
