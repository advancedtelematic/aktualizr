#include "ostree.h"

OstreePackage::OstreePackage(const std::string &ecu_serial_in, const std::string &ref_name_in,
                             const std::string &branch_name_in, const std::string &refhash_in,
                             const std::string &desc_in, const std::string &treehub_in)
    : ecu_serial(ecu_serial_in),
      ref_name(ref_name_in),
      branch_name(branch_name_in),
      refhash(refhash_in),
      description(desc_in),
      pull_uri(treehub_in) {}

data::InstallOutcome OstreePackage::install(const data::PackageManagerCredentials &cred, OstreeConfig config) const {
  (void)cred;
  (void)config;
  return data::InstallOutcome(data::OK, "Good");
}

Json::Value OstreePackage::toEcuVersion(const Json::Value &custom) const {
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

OstreePackage OstreePackage::getEcu(const std::string &ecu_serial,
                                    const std::string &ostree_sysroot __attribute__((unused))) {
  return OstreePackage(ecu_serial, "frgfdg-hash", "fr", "hash", "dsfsdf", "sfsdfs");
}

Json::Value Ostree::getInstalledPackages(const std::string &file_path) {
  (void)file_path;
  Json::Value packages(Json::arrayValue);
  Json::Value package;
  package["name"] = "vim";
  package["version"] = "1.0";
  packages.append(package);

  return packages;
}
