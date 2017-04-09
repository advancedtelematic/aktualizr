#include "ostree.h"
#include <stdio.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <fstream>
#include "logger.h"

#include <gio/gio.h>

OstreeSysroot * Ostree::LoadSysroot(const std::string &path) {
  OstreeSysroot *sysroot = NULL;

  if(path.size()){
    sysroot = ostree_sysroot_new(g_file_new_for_path(path.c_str()));
  }else{
    sysroot = ostree_sysroot_new_default();
  }
  GCancellable *cancellable = NULL;
  GError **error = NULL;
  if (!ostree_sysroot_load(sysroot, cancellable, error)) throw std::runtime_error("could not load sysroot");
  return sysroot;
}

OstreeDeployment* Ostree::getBootedDeployment() {
  
  OstreeSysroot *sysroot = Ostree::LoadSysroot("");
  OstreeRepo *repo = NULL;
  OstreeDeployment *booted_deployment = NULL;

  GCancellable *cancellable = NULL;
  GError **error = NULL;
  
  if (!ostree_sysroot_get_repo(sysroot, &repo, cancellable, error)) throw std::runtime_error("could not get repo");

  booted_deployment = ostree_sysroot_get_booted_deployment (sysroot);
  //booted_deployment = (OstreeDeployment *)ostree_sysroot_get_deployments(sysroot)->pdata[0];
  return booted_deployment;
}

bool Ostree::addRemote(OstreeRepo *repo, const std::string &remote, const std::string &url, const data::PackageManagerCredentials &cred) {
  GCancellable *cancellable = NULL;
  GError *error = NULL;
  GVariantBuilder b;
  GVariant *options;
  
  g_variant_builder_init (&b, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&b, "{s@v}", "gpg-verify", g_variant_new_variant(g_variant_new_boolean (FALSE)));
  
  if(cred.cert_file.size() && cred.pkey_file.size() && cred.ca_file.size()){
     g_variant_builder_add (&b, "{s@v}", "tls-client-cert-path", g_variant_new_variant (g_variant_new_string (cred.cert_file.c_str())));
     g_variant_builder_add (&b, "{s@v}", "tls-client-key-path", g_variant_new_variant (g_variant_new_string (cred.pkey_file.c_str())));
     g_variant_builder_add (&b, "{s@v}", "tls-ca-path", g_variant_new_variant (g_variant_new_string (cred.ca_file.c_str())));
  }
  options = g_variant_builder_end (&b);
  
  if (!ostree_repo_remote_change (repo, NULL,
                                  OSTREE_REPO_REMOTE_CHANGE_DELETE_IF_EXISTS,
                                  remote.c_str(), url.c_str(),
                                  options,
                                  cancellable, &error)){
    LOGGER_LOG(LVL_error, "Error of adding remote: " << error->message);
    return false;
  }
  if (!ostree_repo_remote_change (repo, NULL,
                                  OSTREE_REPO_REMOTE_CHANGE_ADD_IF_NOT_EXISTS,
                                  remote.c_str(), url.c_str(),
                                  options,
                                  cancellable, &error)){
    LOGGER_LOG(LVL_error, "Error of adding remote: " << error->message);
    return false;
  }
  return true;
}

#include "ostree-1/ostree.h"

OstreePackage::OstreePackage(const std::string &ecu_serial_in, const std::string &ref_name_in,
                             const std::string &commit_in, const std::string &desc_in, const std::string &treehub_in)
    : ecu_serial(ecu_serial_in), ref_name(ref_name_in), commit(commit_in), description(desc_in), pull_uri(treehub_in) {}

data::InstallOutcome OstreePackage::install(const data::PackageManagerCredentials &cred) {
  const char remote[] = "aktualizr-remote";
  const char* const refs[] = {commit.c_str()};
  const char* opt_osname = NULL;
  OstreeRepo *repo = NULL;
  GCancellable *cancellable = NULL;
  GError *error = NULL;
  char *revision;
  GVariantBuilder builder;
  GVariant *options;


  OstreeSysroot *sysroot = Ostree::LoadSysroot("");
  if (!ostree_sysroot_get_repo (sysroot, &repo, cancellable, &error)){
    LOGGER_LOG(LVL_error, "could not get repo");
    return data::InstallOutcome(data::INSTALL_FAILED, "could not get repo");
  }

  if (!Ostree::addRemote(repo, remote, pull_uri, cred)){
     return data::InstallOutcome(data::INSTALL_FAILED, "Error of adding remote");
  }


  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&builder, "{s@v}", "flags",
                           g_variant_new_variant (g_variant_new_int32 (0)));
  
  g_variant_builder_add (&builder, "{s@v}", "refs",
                             g_variant_new_variant (g_variant_new_strv (refs, 1)));
                             
  if (cred.access_token.size()) {
    GVariantBuilder hdr_builder;
    std::string av("Bearer ");
    av += cred.access_token;
    g_variant_builder_init (&hdr_builder, G_VARIANT_TYPE ("a(ss)"));
    g_variant_builder_add (&hdr_builder, "(ss)", "Authorization", av.c_str());
    g_variant_builder_add (&builder, "{s@v}", "http-headers",
                               g_variant_new_variant (g_variant_builder_end (&hdr_builder)));
  }
  options = g_variant_ref_sink (g_variant_builder_end (&builder));

  if(!ostree_repo_pull_with_options (repo, remote, options, NULL, cancellable, &error)){
    LOGGER_LOG(LVL_error, "Error of pulling image: " << error->message);
    return data::InstallOutcome(data::INSTALL_FAILED, error->message);
  }

  OstreeDeployment* booted_deployment =  Ostree::getBootedDeployment();
  if(booted_deployment == NULL){
    LOGGER_LOG(LVL_error, "No booted deplyment");
    return data::InstallOutcome(data::INSTALL_FAILED, "No booted deplyment");
  }
  
  GKeyFile *origin = ostree_sysroot_origin_new_from_refspec (sysroot, commit.c_str());
  if (!ostree_repo_resolve_rev (repo, commit.c_str(), FALSE, &revision, &error)){
    LOGGER_LOG(LVL_error, error->message);
    return data::InstallOutcome(data::INSTALL_FAILED, error->message);
  }
  
  OstreeDeployment* merge_deployment = ostree_sysroot_get_merge_deployment (sysroot, opt_osname);
  
  if (!ostree_sysroot_prepare_cleanup (sysroot, cancellable, &error)){
    LOGGER_LOG(LVL_error, error->message);
    return data::InstallOutcome(data::INSTALL_FAILED, error->message);
  }

  std::ifstream args_stream("/proc/cmdline");
  std::string args_content((std::istreambuf_iterator<char>(args_stream)), std::istreambuf_iterator<char>());
  std::vector<std::string> args_vector;
  boost::split(args_vector, args_content, boost::is_any_of(" "));

  std::vector<const char *> kargs_strv_vector;
  kargs_strv_vector.reserve(args_vector.size()+1);

  for (std::vector<std::string>::iterator it = args_vector.begin(); it != args_vector.end(); ++it){
    kargs_strv_vector.push_back((*it).c_str());
  }
  kargs_strv_vector[args_vector.size()] = 0;
  GStrv kargs_strv = const_cast<char**>(&kargs_strv_vector[0]);

  OstreeDeployment *new_deployment = NULL;
  if (!ostree_sysroot_deploy_tree (sysroot,
                                     opt_osname, revision, origin,
                                     merge_deployment, kargs_strv,
                                     &new_deployment,
                                     cancellable, &error)){
    LOGGER_LOG(LVL_error, "ostree_sysroot_deploy_tree: " << error->message);
    return data::InstallOutcome(data::INSTALL_FAILED, error->message);
  }

  if (!ostree_sysroot_simple_write_deployment (sysroot, "",
                                               new_deployment, merge_deployment,
                                              OSTREE_SYSROOT_SIMPLE_WRITE_DEPLOYMENT_FLAGS_RETAIN,
                                               cancellable, &error)){
    LOGGER_LOG(LVL_error, "ostree_sysroot_simple_write_deployment:" << error->message);
    return data::InstallOutcome(data::INSTALL_FAILED, error->message);
 }
  return data::InstallOutcome(data::OK, "Installation succesfull");
}


OstreeBranch OstreeBranch::getCurrent(const std::string &ecu_serial, const std::string &branch) {
  
  OstreeDeployment * booted_deployment = Ostree::getBootedDeployment();
  GKeyFile *origin = ostree_deployment_get_origin(booted_deployment);
  const char *ref = ostree_deployment_get_csum(booted_deployment);
  char *origin_refspec = g_key_file_get_string(origin, "origin", "refspec", NULL);
  OstreePackage package(ecu_serial, (branch + "-") + ref, ref, origin_refspec, "");
  g_free(origin_refspec);
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
      std::ifstream boot_branch_stream(BOOT_BRANCH.c_str());
      std::string branch_name((std::istreambuf_iterator<char>(boot_branch_stream)), std::istreambuf_iterator<char>());
      branch_name.erase(std::remove(branch_name.begin(), branch_name.end(), '\n'), branch_name.end());
      return OstreeBranch::getCurrent(ecu_serial, branch_name).package;
    }
  }
  throw std::runtime_error("unknown current branch");
}
