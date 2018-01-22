#include "ostree.h"

OstreePackage::OstreePackage(const std::string &ref_name_in, const std::string &refhash_in,
                             const std::string &treehub_in)
    : ref_name(ref_name_in), refhash(refhash_in), pull_uri(treehub_in) {}

data::InstallOutcome OstreePackage::install(const OstreeConfig &config) const {
  (void)config;
  return data::InstallOutcome(data::OK, "Good");
}

Json::Value OstreePackage::toEcuVersion(const std::string &ecu_serial, const Json::Value &custom) const {
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

data::InstallOutcome Ostree::pull(const Config &config, const data::PackageManagerCredentials &cred,
                                  const std::string &hash) {
  (void)config;
  (void)cred;
  (void)hash;
  return data::InstallOutcome(data::OK, "Good");
}

Json::Value Ostree::getInstalledPackages(const boost::filesystem::path &file_path) {
  (void)file_path;
  Json::Value packages(Json::arrayValue);
  Json::Value package;
  package["name"] = "vim";
  package["version"] = "1.0";
  packages.append(package);

  return packages;
}

std::string OstreePackage::getCurrent(const boost::filesystem::path &ostree_sysroot) {
  (void)ostree_sysroot;
  return "hash";
}
