#include "ostree.h"

#include <stdio.h>
#include <unistd.h>
#include <fstream>

#include <gio/gio.h>
#include <json/json.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>

#include "logging.h"
#include "utils.h"

OstreePackage::OstreePackage(const std::string &ref_name_in, const std::string &refhash_in,
                             const std::string &treehub_in)
    : PackageInterface(ref_name_in, refhash_in, treehub_in) {}

data::InstallOutcome OstreeManager::pull(const Config &config, const data::PackageManagerCredentials &cred,
                                         const std::string &refhash) {
  OstreeRepo *repo = NULL;
  const char *const commit_ids[] = {refhash.c_str()};
  GCancellable *cancellable = NULL;
  GError *error = NULL;
  GVariantBuilder builder;
  GVariant *options;

  boost::shared_ptr<OstreeSysroot> sysroot = OstreeManager::LoadSysroot(config.pacman.sysroot);
  if (!ostree_sysroot_get_repo(sysroot.get(), &repo, cancellable, &error)) {
    LOG_ERROR << "Could not get OSTree repo";
    g_error_free(error);
    return data::InstallOutcome(data::INSTALL_FAILED, "Could not get OSTree repo");
  }

  if (!OstreeManager::addRemote(repo, config.uptane.ostree_server, cred)) {
    g_object_unref(repo);
    return data::InstallOutcome(data::INSTALL_FAILED, "Error of adding remote");
  }

  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{s@v}", "flags", g_variant_new_variant(g_variant_new_int32(0)));

  g_variant_builder_add(&builder, "{s@v}", "refs", g_variant_new_variant(g_variant_new_strv(commit_ids, 1)));

  options = g_variant_builder_end(&builder);

  if (!ostree_repo_pull_with_options(repo, remote, options, NULL, cancellable, &error)) {
    LOG_ERROR << "Error of pulling image: " << error->message;
    data::InstallOutcome install_outcome(data::INSTALL_FAILED, error->message);
    g_error_free(error);
    g_object_unref(repo);
    g_variant_unref(options);
    return install_outcome;
  }
  g_object_unref(repo);
  g_variant_unref(options);
  return data::InstallOutcome(data::OK, "Pulling ostree image was successful");
}

data::InstallOutcome OstreePackage::install(const PackageConfig &config) const {
  const char *opt_osname = NULL;
  OstreeRepo *repo = NULL;
  GCancellable *cancellable = NULL;
  GError *error = NULL;
  char *revision;

  if (config.os.size()) {
    opt_osname = config.os.c_str();
  }

  boost::shared_ptr<OstreeSysroot> sysroot = OstreeManager::LoadSysroot(config.sysroot);

  if (!ostree_sysroot_get_repo(sysroot.get(), &repo, cancellable, &error)) {
    LOG_ERROR << "could not get repo";
    g_error_free(error);
    return data::InstallOutcome(data::INSTALL_FAILED, "could not get repo");
  }

  GKeyFile *origin = ostree_sysroot_origin_new_from_refspec(sysroot.get(), refhash.c_str());
  if (!ostree_repo_resolve_rev(repo, refhash.c_str(), FALSE, &revision, &error)) {
    LOG_ERROR << error->message;
    data::InstallOutcome install_outcome(data::INSTALL_FAILED, error->message);
    g_error_free(error);
    return install_outcome;
  }

  OstreeDeployment *merge_deployment = ostree_sysroot_get_merge_deployment(sysroot.get(), opt_osname);
  if (merge_deployment == NULL) {
    LOG_ERROR << "No merge deployment";
    return data::InstallOutcome(data::INSTALL_FAILED, "No merge deployment");
  }

  if (!ostree_sysroot_prepare_cleanup(sysroot.get(), cancellable, &error)) {
    LOG_ERROR << error->message;
    data::InstallOutcome install_outcome(data::INSTALL_FAILED, error->message);
    g_error_free(error);
    return install_outcome;
  }

  std::string args_content =
      std::string(ostree_bootconfig_parser_get(ostree_deployment_get_bootconfig(merge_deployment), "options"));
  std::vector<std::string> args_vector;
  boost::split(args_vector, args_content, boost::is_any_of(" "));

  std::vector<const char *> kargs_strv_vector;
  kargs_strv_vector.reserve(args_vector.size() + 1);

  for (std::vector<std::string>::iterator it = args_vector.begin(); it != args_vector.end(); ++it) {
    kargs_strv_vector.push_back((*it).c_str());
  }
  kargs_strv_vector[args_vector.size()] = 0;
  GStrv kargs_strv = const_cast<char **>(&kargs_strv_vector[0]);

  OstreeDeployment *new_deployment = NULL;
  if (!ostree_sysroot_deploy_tree(sysroot.get(), opt_osname, revision, origin, merge_deployment, kargs_strv,
                                  &new_deployment, cancellable, &error)) {
    LOG_ERROR << "ostree_sysroot_deploy_tree: " << error->message;
    data::InstallOutcome install_outcome(data::INSTALL_FAILED, error->message);
    g_error_free(error);
    return install_outcome;
  }

  if (!ostree_sysroot_simple_write_deployment(sysroot.get(), NULL, new_deployment, merge_deployment,
                                              OSTREE_SYSROOT_SIMPLE_WRITE_DEPLOYMENT_FLAGS_NONE, cancellable, &error)) {
    LOG_ERROR << "ostree_sysroot_simple_write_deployment:" << error->message;
    data::InstallOutcome install_outcome(data::INSTALL_FAILED, error->message);
    g_error_free(error);
    return install_outcome;
  }
  LOG_INFO << "Performing sync()";
  sync();
  return data::InstallOutcome(data::OK, "Installation successful");
}

Json::Value OstreePackage::toEcuVersion(const std::string &ecu_serial, const Json::Value &custom) const {
  Json::Value installed_image;
  installed_image["filepath"] = ref_name;
  installed_image["fileinfo"]["length"] = 0;
  installed_image["fileinfo"]["hashes"]["sha256"] = refhash;

  Json::Value value;
  value["attacks_detected"] = "";
  value["installed_image"] = installed_image;
  value["ecu_serial"] = ecu_serial;
  value["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
  value["timeserver_time"] = "1970-01-01T00:00:00Z";
  if (custom != Json::nullValue) {
    value["custom"] = custom;
  }
  return value;
}

OstreeManager::OstreeManager(const PackageConfig &pconfig) {
  sysroot_dir = pconfig.sysroot;
  packages_file = pconfig.packages_file;
  try {
    getCurrent();
  } catch (...) {
    throw std::runtime_error("Could not find OSTree sysroot at: " + sysroot_dir.string());
  }
}

Json::Value OstreeManager::getInstalledPackages() {
  std::string packages_str = Utils::readFile(packages_file);
  std::vector<std::string> package_lines;
  boost::split(package_lines, packages_str, boost::is_any_of("\n"));
  Json::Value packages(Json::arrayValue);
  for (std::vector<std::string>::iterator it = package_lines.begin(); it != package_lines.end(); ++it) {
    if (it->empty()) {
      continue;
    }
    size_t pos = it->find(" ");
    if (pos == std::string::npos) {
      throw std::runtime_error("Wrong packages file format");
    }
    Json::Value package;
    package["name"] = it->substr(0, pos);
    package["version"] = it->substr(pos + 1);
    packages.append(package);
  }
  return packages;
}

std::string OstreeManager::getCurrent() {
  boost::shared_ptr<OstreeDeployment> staged_deployment = getStagedDeployment();
  if (!staged_deployment) {
    throw std::runtime_error("No deployments found in OSTree sysroot at: " + sysroot_dir.string());
  }
  return ostree_deployment_get_csum(staged_deployment.get());
}

boost::shared_ptr<PackageInterface> OstreeManager::makePackage(const std::string &branch_name_in,
                                                               const std::string &refhash_in,
                                                               const std::string &treehub_in) {
  return boost::make_shared<OstreePackage>(branch_name_in, refhash_in, treehub_in);
}

boost::shared_ptr<OstreeDeployment> OstreeManager::getStagedDeployment() {
  boost::shared_ptr<OstreeSysroot> sysroot_smart = OstreeManager::LoadSysroot(sysroot_dir);

  GPtrArray *deployments = NULL;
  boost::shared_ptr<OstreeDeployment> res;

  deployments = ostree_sysroot_get_deployments(sysroot_smart.get());

  if (deployments->len > 0) {
    OstreeDeployment *d = static_cast<OstreeDeployment *>(deployments->pdata[0]);
    OstreeDeployment *d2 = static_cast<OstreeDeployment *>(g_object_ref(d));
    res = boost::shared_ptr<OstreeDeployment>(d2, g_object_unref);
  }

  g_ptr_array_unref(deployments);
  return res;
}

boost::shared_ptr<OstreeSysroot> OstreeManager::LoadSysroot(const boost::filesystem::path &path) {
  OstreeSysroot *sysroot = NULL;

  if (!path.empty()) {
    GFile *fl = g_file_new_for_path(path.c_str());
    sysroot = ostree_sysroot_new(fl);
    g_object_unref(fl);
  } else {
    sysroot = ostree_sysroot_new_default();
  }
  GError *error = NULL;
  if (!ostree_sysroot_load(sysroot, NULL, &error)) {
    if (error != NULL) {
      g_error_free(error);
    }
    g_object_unref(sysroot);
    throw std::runtime_error("could not load sysroot");
  }
  return boost::shared_ptr<OstreeSysroot>(sysroot, g_object_unref);
}

bool OstreeManager::addRemote(OstreeRepo *repo, const std::string &url, const data::PackageManagerCredentials &cred) {
  GCancellable *cancellable = NULL;
  GError *error = NULL;
  GVariantBuilder b;
  GVariant *options;

  g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&b, "{s@v}", "gpg-verify", g_variant_new_variant(g_variant_new_boolean(FALSE)));

  if (cred.cert_file.size() && cred.pkey_file.size() && cred.ca_file.size()) {
    g_variant_builder_add(&b, "{s@v}", "tls-client-cert-path",
                          g_variant_new_variant(g_variant_new_string(cred.cert_file.c_str())));
    g_variant_builder_add(&b, "{s@v}", "tls-client-key-path",
                          g_variant_new_variant(g_variant_new_string(cred.pkey_file.c_str())));
    g_variant_builder_add(&b, "{s@v}", "tls-ca-path",
                          g_variant_new_variant(g_variant_new_string(cred.ca_file.c_str())));
  }
  options = g_variant_builder_end(&b);

  if (!ostree_repo_remote_change(repo, NULL, OSTREE_REPO_REMOTE_CHANGE_DELETE_IF_EXISTS, remote, url.c_str(), options,
                                 cancellable, &error)) {
    LOG_ERROR << "Error of adding remote: " << error->message;
    g_error_free(error);
    g_variant_unref(options);
    return false;
  }
  if (!ostree_repo_remote_change(repo, NULL, OSTREE_REPO_REMOTE_CHANGE_ADD_IF_NOT_EXISTS, remote, url.c_str(), options,
                                 cancellable, &error)) {
    LOG_ERROR << "Error of adding remote: " << error->message;
    g_error_free(error);
    g_variant_unref(options);
    return false;
  }
  g_variant_unref(options);
  return true;
}
