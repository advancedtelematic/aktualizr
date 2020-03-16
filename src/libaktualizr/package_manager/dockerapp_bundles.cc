#include "dockerappmanager.h"
#include "packagemanagerfactory.h"

#include <sstream>

/**
 * @brief This package manager compliments the OSTreePackageManager by also including optional Docker Apps.
 *
 * A full description of the Docker App project can be found here:
 *  https://github.com/docker/app/
 *
 * Docker Apps are very analogous to docker-compose. In fact, this module
 * currently "renders" the docker-app file into a docker-compose file. Each
 * Docker App appears as a custom metadata for a Target in the TUF targets list.
 *
 *   "targets": {
 *    "raspberrypi3-64-lmp-144" : {
 *      "custom" : {
 *         "docker_apps" : {
 *            "httpd" : {
 *               "uri" :
 * "hub.foundries.io/andy-corp/shellhttpd@sha256:611052af5d819ea979bd555b508808c8a84f9159130d455ed52a084ab5b6b398"
 *            }
 *         },
 *         "hardwareIds" : ["raspberrypi3-64"],
 *         "name" : "raspberrypi3-64-lmp",
 *         "targetFormat" : "OSTREE",
 *         "version" : "144"
 *      },
 *      "hashes" : {"sha256" : "20ac4f7cd50cda6bfed0caa1f8231cc9a7e40bec60026c66df5f7e143af96942"},
 *      "length" : 0
 *     }
 *   }
 */

struct AppBundle {
  AppBundle(std::string app_name, const DockerAppManagerConfig &config)
      : name(std::move(app_name)),
        app_root(config.docker_apps_root / name),
        app_params(config.docker_app_params),
        compose_bin(boost::filesystem::canonical(config.docker_compose_bin).string()) {}

  // Utils::shell isn't interactive. The docker app commands can take a few
  // seconds to run, so we use std::system to stream it to stdout/sterr
  bool cmd_streaming(const std::string &cmd) { return std::system(cmd.c_str()) == 0; }

  bool fetch(const std::string &app_uri) { return cmd_streaming("docker app pull " + app_uri); }

  bool start(const std::string &app_uri) {
    boost::filesystem::create_directories(app_root);
    std::string cmd("cd " + app_root.string() + " && docker app image render -o docker-compose.yml " + app_uri);
    if (!app_params.empty()) {
      cmd += " --parameters-file " + app_params.string();
    }
    if (!cmd_streaming(cmd)) {
      return false;
    }

    return cmd_streaming("cd " + app_root.string() + " && " + compose_bin + " up --remove-orphans -d");
  }

  void remove() {
    if (cmd_streaming("cd " + app_root.string() + " && " + compose_bin + " down")) {
      boost::filesystem::remove_all(app_root);
    } else {
      LOG_ERROR << "docker-compose was unable to bring down: " << app_root;
    }
  }

  std::string name;
  boost::filesystem::path app_root;
  boost::filesystem::path app_params;
  std::string compose_bin;
};

std::vector<std::pair<std::string, std::string>> DockerAppBundles::iterate_apps(const Uptane::Target &target) const {
  auto apps = target.custom_data()["docker_apps"];
  Uptane::ImageRepository repo;
  // checkMetaOffline pulls in data from INvStorage to properly initialize
  // the targets member of the instance so that we can use the LazyTargetList
  repo.checkMetaOffline(*storage_);

  if (!apps) {
    LOG_DEBUG << "Detected an update target from Director with no docker-apps data";
    for (const auto &t : Uptane::LazyTargetsList(repo, storage_, fake_fetcher_)) {
      if (t.MatchTarget(target)) {
        LOG_DEBUG << "Found the match " << t;
        apps = t.custom_data()["docker_apps"];
        break;
      }
    }
  }

  std::vector<std::pair<std::string, std::string>> bundles;
  for (Json::ValueIterator i = apps.begin(); i != apps.end(); ++i) {
    if ((*i).isObject() && (*i).isMember("uri")) {
      for (const auto &app : dappcfg_.docker_apps) {
        if (i.key().asString() == app) {
          bundles.emplace_back(i.key().asString(), (*i)["uri"].asString());
          break;
        }
      }
    } else if ((*i).isObject() && (*i).isMember("filename")) {
      LOG_TRACE << "Skipping old docker app format for " << i.key().asString() << " -> " << *i;
    } else {
      LOG_ERROR << "Invalid custom data for docker-app: " << i.key().asString() << " -> " << *i;
    }
  }
  return bundles;
}

bool DockerAppBundles::fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                                   FetcherProgressCb progress_cb, const api::FlowControlToken *token) {
  if (!OstreeManager::fetchTarget(target, fetcher, keys, progress_cb, token)) {
    return false;
  }

  LOG_INFO << "Looking for DockerApps to fetch";
  bool passed = true;
  for (const auto &pair : iterate_apps(target)) {
    LOG_INFO << "Fetching " << pair.first << " -> " << pair.second;
    if (!AppBundle(pair.first, dappcfg_).fetch(pair.second)) {
      passed = false;
    }
  }
  return passed;
}

data::InstallationResult DockerAppBundles::install(const Uptane::Target &target) const {
  auto res = OstreeManager::install(target);
  handleRemovedApps(target);
  for (const auto &pair : iterate_apps(target)) {
    LOG_INFO << "Installing " << pair.first << " -> " << pair.second;
    if (!AppBundle(pair.first, dappcfg_).start(pair.second)) {
      res = data::InstallationResult(data::ResultCode::Numeric::kInstallFailed, "Could not install docker app");
    }
  };

  if (dappcfg_.docker_prune) {
    LOG_INFO << "Pruning unused docker images";
    // Utils::shell which isn't interactive, we'll use std::system so that
    // stdout/stderr is streamed while docker sets things up.
    if (std::system("docker image prune -a -f --filter=\"label!=aktualizr-no-prune\"") != 0) {
      LOG_WARNING << "Unable to prune unused docker images";
    }
  }

  return res;
}

// Handle the case like:
//  1) sota.toml is configured with 2 docker apps: "app1, app2"
//  2) update is applied, so we are now running both app1 and app2
//  3) sota.toml is updated with 1 docker app: "app1"
// At this point we should stop app2 and remove it.
void DockerAppBundles::handleRemovedApps(const Uptane::Target &target) const {
  if (!boost::filesystem::is_directory(dappcfg_.docker_apps_root)) {
    LOG_DEBUG << "dappcfg_.docker_apps_root does not exist";
    return;
  }

  std::vector<std::string> target_apps = target.custom_data()["docker_apps"].getMemberNames();

  for (auto &entry : boost::make_iterator_range(boost::filesystem::directory_iterator(dappcfg_.docker_apps_root), {})) {
    if (boost::filesystem::is_directory(entry)) {
      std::string name = entry.path().filename().native();
      if (std::find(dappcfg_.docker_apps.begin(), dappcfg_.docker_apps.end(), name) == dappcfg_.docker_apps.end()) {
        LOG_WARNING << "Docker App(" << name
                    << ") installed, but is now removed from configuration. Removing from system";
        AppBundle(name, dappcfg_).remove();
      }
      if (std::find(target_apps.begin(), target_apps.end(), name) == target_apps.end()) {
        LOG_WARNING << "Docker App(" << name
                    << ") configured, but not defined in installation target. Removing from system";
        AppBundle(name, dappcfg_).remove();
      }
    }
  }
}
