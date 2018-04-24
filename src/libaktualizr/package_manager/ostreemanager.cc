#include "package_manager/ostreemanager.h"

#include <stdio.h>
#include <unistd.h>
#include <fstream>

#include <gio/gio.h>
#include <json/json.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <utility>

#include "logging/logging.h"
#include "utilities/utils.h"

data::InstallOutcome OstreeManager::pull(const boost::filesystem::path &sysroot_path, const std::string &ostree_server,
                                         const KeyManager &keys, const std::string &refhash) {
  const char *const commit_ids[] = {refhash.c_str()};
  GCancellable *cancellable = nullptr;
  GError *error = nullptr;
  GVariantBuilder builder;
  GVariant *options;

  std::shared_ptr<OstreeSysroot> sysroot = OstreeManager::LoadSysroot(sysroot_path);
  std::shared_ptr<OstreeRepo> repo = LoadRepo(sysroot, &error);
  if (error != nullptr) {
    LOG_ERROR << "Could not get OSTree repo";
    g_error_free(error);
    return data::InstallOutcome(data::INSTALL_FAILED, "Could not get OSTree repo");
  }

  GHashTable *ref_list = nullptr;
  if (ostree_repo_list_commit_objects_starting_with(repo.get(), refhash.c_str(), &ref_list, nullptr, &error) != 0) {
    guint length = g_hash_table_size(ref_list);
    g_hash_table_destroy(ref_list);  // OSTree creates the table with destroy notifiers, so no memory leaks expected
    // should never be greater than 1, but use >= for robustness
    if (length >= 1) {
      return data::InstallOutcome(data::ALREADY_PROCESSED, "Refhash was already pulled");
    }
  }
  if (error != nullptr) {
    g_error_free(error);
    error = nullptr;
  }

  if (!OstreeManager::addRemote(repo.get(), ostree_server, keys)) {
    return data::InstallOutcome(data::INSTALL_FAILED, "Error adding OSTree remote");
  }

  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&builder, "{s@v}", "flags", g_variant_new_variant(g_variant_new_int32(0)));

  g_variant_builder_add(&builder, "{s@v}", "refs", g_variant_new_variant(g_variant_new_strv(commit_ids, 1)));

  options = g_variant_builder_end(&builder);

  if (ostree_repo_pull_with_options(repo.get(), remote, options, nullptr, cancellable, &error) == 0) {
    LOG_ERROR << "Error of pulling image: " << error->message;
    data::InstallOutcome install_outcome(data::INSTALL_FAILED, error->message);
    g_error_free(error);
    g_variant_unref(options);
    return install_outcome;
  }
  g_variant_unref(options);
  return data::InstallOutcome(data::OK, "Pulling ostree image was successful");
}

data::InstallOutcome OstreeManager::install(const Uptane::Target &target) const {
  const char *opt_osname = nullptr;
  GCancellable *cancellable = nullptr;
  GError *error = nullptr;
  char *revision;

  if (config.os.size() != 0u) {
    opt_osname = config.os.c_str();
  }

  std::shared_ptr<OstreeSysroot> sysroot = OstreeManager::LoadSysroot(config.sysroot);
  std::shared_ptr<OstreeRepo> repo = LoadRepo(sysroot, &error);

  if (error != nullptr) {
    LOG_ERROR << "could not get repo";
    g_error_free(error);
    return data::InstallOutcome(data::INSTALL_FAILED, "could not get repo");
  }

  GKeyFile *origin = ostree_sysroot_origin_new_from_refspec(sysroot.get(), target.sha256Hash().c_str());
  if (ostree_repo_resolve_rev(repo.get(), target.sha256Hash().c_str(), FALSE, &revision, &error) == 0) {
    LOG_ERROR << error->message;
    data::InstallOutcome install_outcome(data::INSTALL_FAILED, error->message);
    g_error_free(error);
    return install_outcome;
  }

  OstreeDeployment *merge_deployment = ostree_sysroot_get_merge_deployment(sysroot.get(), opt_osname);
  if (merge_deployment == nullptr) {
    LOG_ERROR << "No merge deployment";
    return data::InstallOutcome(data::INSTALL_FAILED, "No merge deployment");
  }

  if (ostree_sysroot_prepare_cleanup(sysroot.get(), cancellable, &error) == 0) {
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

  for (auto it = args_vector.begin(); it != args_vector.end(); ++it) {
    kargs_strv_vector.push_back((*it).c_str());
  }
  kargs_strv_vector[args_vector.size()] = nullptr;
  auto kargs_strv = const_cast<char **>(&kargs_strv_vector[0]);

  OstreeDeployment *new_deployment = nullptr;
  if (ostree_sysroot_deploy_tree(sysroot.get(), opt_osname, revision, origin, merge_deployment, kargs_strv,
                                 &new_deployment, cancellable, &error) == 0) {
    LOG_ERROR << "ostree_sysroot_deploy_tree: " << error->message;
    data::InstallOutcome install_outcome(data::INSTALL_FAILED, error->message);
    g_error_free(error);
    return install_outcome;
  }

  if (ostree_sysroot_simple_write_deployment(sysroot.get(), nullptr, new_deployment, merge_deployment,
                                             OSTREE_SYSROOT_SIMPLE_WRITE_DEPLOYMENT_FLAGS_NONE, cancellable,
                                             &error) == 0) {
    LOG_ERROR << "ostree_sysroot_simple_write_deployment:" << error->message;
    data::InstallOutcome install_outcome(data::INSTALL_FAILED, error->message);
    g_error_free(error);
    return install_outcome;
  }
  LOG_INFO << "Performing sync()";
  sync();
  return data::InstallOutcome(data::OK, "Installation successful");
}

OstreeManager::OstreeManager(PackageConfig pconfig, std::shared_ptr<INvStorage> storage)
    : config(std::move(pconfig)), storage_(std::move(storage)) {
  try {
    OstreeManager::getCurrent();
  } catch (...) {
    throw std::runtime_error("Could not find OSTree sysroot at: " + config.sysroot.string());
  }
}

Json::Value OstreeManager::getInstalledPackages() {
  std::string packages_str = Utils::readFile(config.packages_file);
  std::vector<std::string> package_lines;
  boost::split(package_lines, packages_str, boost::is_any_of("\n"));
  Json::Value packages(Json::arrayValue);
  for (auto it = package_lines.begin(); it != package_lines.end(); ++it) {
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

Uptane::Target OstreeManager::getCurrent() {
  std::shared_ptr<OstreeDeployment> staged_deployment = getStagedDeployment();
  if (!staged_deployment) {
    throw std::runtime_error("No deployments found in OSTree sysroot at: " + config.sysroot.string());
  }
  std::string current_hash = ostree_deployment_get_csum(staged_deployment.get());

  std::vector<Uptane::Target> installed_versions;
  storage_->loadInstalledVersions(&installed_versions);

  std::vector<Uptane::Target>::iterator it;
  for (it = installed_versions.begin(); it != installed_versions.end(); it++) {
    if (it->sha256Hash() == current_hash) {
      return *it;
    }
  }

  return getUnknown();
}

std::shared_ptr<OstreeDeployment> OstreeManager::getStagedDeployment() {
  std::shared_ptr<OstreeSysroot> sysroot_smart = OstreeManager::LoadSysroot(config.sysroot);

  GPtrArray *deployments = nullptr;
  std::shared_ptr<OstreeDeployment> res;

  deployments = ostree_sysroot_get_deployments(sysroot_smart.get());

  if (deployments->len > 0) {
    auto *d = static_cast<OstreeDeployment *>(deployments->pdata[0]);
    auto *d2 = static_cast<OstreeDeployment *>(g_object_ref(d));
    res = std::shared_ptr<OstreeDeployment>(d2, g_object_unref);
  }

  g_ptr_array_unref(deployments);
  return res;
}

std::shared_ptr<OstreeSysroot> OstreeManager::LoadSysroot(const boost::filesystem::path &path) {
  OstreeSysroot *sysroot = nullptr;

  if (!path.empty()) {
    GFile *fl = g_file_new_for_path(path.c_str());
    sysroot = ostree_sysroot_new(fl);
    g_object_unref(fl);
  } else {
    sysroot = ostree_sysroot_new_default();
  }
  GError *error = nullptr;
  if (ostree_sysroot_load(sysroot, nullptr, &error) == 0) {
    if (error != nullptr) {
      g_error_free(error);
    }
    g_object_unref(sysroot);
    throw std::runtime_error("could not load sysroot");
  }
  return std::shared_ptr<OstreeSysroot>(sysroot, g_object_unref);
}

std::shared_ptr<OstreeRepo> OstreeManager::LoadRepo(const std::shared_ptr<OstreeSysroot> &sysroot, GError **error) {
  OstreeRepo *repo = nullptr;

  if (ostree_sysroot_get_repo(sysroot.get(), &repo, nullptr, error) == 0) {
    return std::shared_ptr<OstreeRepo>();
  }

  return std::shared_ptr<OstreeRepo>(repo, g_object_unref);
}

bool OstreeManager::addRemote(OstreeRepo *repo, const std::string &url, const KeyManager &keys) {
  GCancellable *cancellable = nullptr;
  GError *error = nullptr;
  GVariantBuilder b;
  GVariant *options;

  g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&b, "{s@v}", "gpg-verify", g_variant_new_variant(g_variant_new_boolean(FALSE)));

  std::string cert_file = keys.getCertFile();
  std::string pkey_file = keys.getPkeyFile();
  std::string ca_file = keys.getCaFile();
  if (!cert_file.empty() && !pkey_file.empty() && !ca_file.empty()) {
    g_variant_builder_add(&b, "{s@v}", "tls-client-cert-path",
                          g_variant_new_variant(g_variant_new_string(cert_file.c_str())));
    g_variant_builder_add(&b, "{s@v}", "tls-client-key-path",
                          g_variant_new_variant(g_variant_new_string(pkey_file.c_str())));
    g_variant_builder_add(&b, "{s@v}", "tls-ca-path", g_variant_new_variant(g_variant_new_string(ca_file.c_str())));
  }
  options = g_variant_builder_end(&b);

  if (ostree_repo_remote_change(repo, nullptr, OSTREE_REPO_REMOTE_CHANGE_DELETE_IF_EXISTS, remote, url.c_str(), options,
                                cancellable, &error) == 0) {
    LOG_ERROR << "Error of adding remote: " << error->message;
    g_error_free(error);
    g_variant_unref(options);
    return false;
  }
  if (ostree_repo_remote_change(repo, nullptr, OSTREE_REPO_REMOTE_CHANGE_ADD_IF_NOT_EXISTS, remote, url.c_str(),
                                options, cancellable, &error) == 0) {
    LOG_ERROR << "Error of adding remote: " << error->message;
    g_error_free(error);
    g_variant_unref(options);
    return false;
  }
  g_variant_unref(options);
  return true;
}
