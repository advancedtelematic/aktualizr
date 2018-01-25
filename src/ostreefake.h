#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "ostreeinterface.h"

class OstreeFakePackage : public OstreePackageInterface {
 public:
  OstreeFakePackage(const std::string &ref_name_in, const std::string &refhash_in, const std::string &treehub_in)
      : OstreePackageInterface(ref_name_in, refhash_in, treehub_in) {}

  ~OstreeFakePackage() {}

  data::InstallOutcome install(const data::PackageManagerCredentials &cred, const PackageConfig &pconfig) const {
    (void)cred;
    (void)pconfig;
    return data::InstallOutcome(data::OK, "Good");
  }

  Json::Value toEcuVersion(const std::string &ecu_serial, const Json::Value &custom) const {
    Json::Value installed_image;
    installed_image["filepath"] = ref_name;
    installed_image["fileinfo"]["length"] = 0;
    installed_image["fileinfo"]["hashes"]["sha256"] = refhash;
    installed_image["fileinfo"]["custom"] = false;

    Json::Value value;
    value["attacks_detected"] = "";
    value["installed_image"] = installed_image;
    value["ecu_serial"] = ecu_serial;
    value["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
    value["timeserver_time"] = "1970-01-01T00:00:00Z";
    if (custom != Json::nullValue) {
      value["custom"] = custom;
    } else {
      value["custom"] = false;
    }
    return value;
  }
};

class OstreeFakeManager : public OstreeManagerInterface {
 public:
  Json::Value getInstalledPackages() {
    Json::Value packages(Json::arrayValue);
    Json::Value package;
    package["name"] = "vim";
    package["version"] = "1.0";
    packages.append(package);

    return packages;
  }

  std::string getCurrent() { return "hash"; }

  boost::shared_ptr<PackageInterface> makePackage(const std::string &branch_name_in, const std::string &refhash_in,
                                                  const std::string &treehub_in) {
    return boost::make_shared<OstreeFakePackage>(branch_name_in, refhash_in, treehub_in);
  }
};
