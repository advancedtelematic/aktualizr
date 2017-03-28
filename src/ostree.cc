#include "ostree.h"
#include <stdio.h>
#include <boost/filesystem.hpp>
#include <fstream>

OstreePackage::OstreePackage(const std::string &ecu_serial_in, const std::string &ref_name_in,
                             const std::string &commit_in, const std::string &desc_in, const std::string &treehub_in)
    : ecu_serial(ecu_serial_in), ref_name(ref_name_in), commit(commit_in), description(desc_in), pull_uri(treehub_in) {}

InstallOutcome OstreePackage::install(const data::PackageManagerCredentials &cred) {
  FILE *pipe;
  char buff[512];
  std::string env;
  env += "COMMIT=" + commit;
  env += " REF_NAME='" + ref_name + "'";
  env += " DESCRIPTION='" + description + "'";
  env += " PULL_URI='" + pull_uri + "'";
  if (cred.access_token.size()) {
    env += " AUTHPLUS_ACCESS_TOKEN='" + cred.access_token + "'";
  }
  if (cred.ca_file.size()) {
    env += " TLS_CA_CERT='" + cred.ca_file + "'";
  }
  if (cred.cert_file.size()) {
    env += " TLS_CLIENT_CERT='" + cred.cert_file + "'";
  }
  if (cred.pkey_file.size()) {
    env += " TLS_CLIENT_KEY='" + cred.pkey_file + "' ";
  }

  std::string output;
  std::string ostree_script = "sota_ostree.sh";
  if (!(pipe = popen((env + ostree_script).c_str(), "r"))) {
    throw std::runtime_error("could not find or execute: " + ostree_script);
  }
  while (fgets(buff, sizeof(buff), pipe) != NULL) {
    output += buff;
  }
  int status_code = pclose(pipe) / 256;
  if (status_code == 0) {
    // FIXME write json to NEW_PACKAGE
    return InstallOutcome(data::OK, output);
  } else if (status_code == 99) {
    return InstallOutcome(data::ALREADY_PROCESSED, output);
  }
  return InstallOutcome(data::INSTALL_FAILED, output);
}

OstreeBranch OstreeBranch::getCurrent(const std::string &ecu_serial, const std::string &branch) {
  OstreeSysroot *sysroot = NULL;
  OstreeRepo *repo = NULL;
  OstreeDeployment *booted_deployment = NULL;
  GKeyFile *origin;

  sysroot = ostree_sysroot_new_default();
  GCancellable *cancellable = NULL;
  GError **error = NULL;
  if (!ostree_sysroot_load(sysroot, cancellable, error)) throw std::runtime_error("could not load sysroot");

  if (!ostree_sysroot_get_repo(sysroot, &repo, cancellable, error)) throw std::runtime_error("could not get repo");

  // booted_deployment = ostree_sysroot_get_booted_deployment (sysroot);
  booted_deployment = (OstreeDeployment *)ostree_sysroot_get_deployments(sysroot)->pdata[0];
  if (!booted_deployment) {
    throw std::logic_error("no booted deployment");
  }

  origin = ostree_deployment_get_origin(booted_deployment);
  const char *ref = ostree_deployment_get_csum(booted_deployment);
  g_autofree char *origin_refspec = g_key_file_get_string(origin, "origin", "refspec", NULL);
  OstreePackage package(ecu_serial, (branch + "-") + ref, ref, origin_refspec, "");
  return OstreeBranch(true, ostree_deployment_get_osname(booted_deployment), package);
}


OstreePackage OstreePackage::fromJson(const Json::Value &json) {
  return OstreePackage(json["ecu_serial"].asString(), json["ref_name"].asString(), json["commit"].asString(),
                       json["description"].asString(), json["pull_uri"].asString());
}

Json::Value OstreePackage::toEcuVersion(const Json::Value &custom) {
  Json::Value installed_image;
  installed_image["filepath"] = ref_name;
  installed_image["fileinfo"]["length"] = 0;
  installed_image["fileinfo"]["hashes"]["sha256"] = commit;
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

OstreePackage OstreePackage::getEcu(const std::string &ecu_serial) {
  if (boost::filesystem::exists(NEW_PACKAGE)) {
    std::ifstream path_stream(NEW_PACKAGE.c_str());
    std::string json_content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
    return OstreePackage::fromJson(json_content);
  } else {
    if (boost::filesystem::exists(BOOT_BRANCH)) {
      std::ifstream boot_branch_stream(NEW_PACKAGE.c_str());
      std::string branch_name((std::istreambuf_iterator<char>(boot_branch_stream)), std::istreambuf_iterator<char>());
      return OstreeBranch::getCurrent(ecu_serial, branch_name).package;
    }
  }
  throw std::runtime_error("unknown current branch");
}
