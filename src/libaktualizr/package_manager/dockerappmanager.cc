#include "dockerappmanager.h"

bool DockerAppManager::iterate_apps(const Uptane::Target &target, DockerAppCb cb) const {
  auto apps = target.custom_data()["docker_apps"];
  bool res = true;
  Uptane::ImagesRepository repo;
  // checkMetaOffline pulls in data from INvStorage to properly initialize
  // the targets member of the instance so that we can use the LazyTargetList
  repo.checkMetaOffline(*storage_);

  if (!apps) {
    LOG_DEBUG << "Detected an update target from Director with no docker-apps data";
    for (const auto t : Uptane::LazyTargetsList(repo, storage_, fake_fetcher_)) {
      if (t == target) {
        LOG_DEBUG << "Found the match " << t;
        apps = t.custom_data()["docker_apps"];
        break;
      }
    }
  }

  for (const auto t : Uptane::LazyTargetsList(repo, storage_, fake_fetcher_)) {
    for (Json::ValueIterator i = apps.begin(); i != apps.end(); ++i) {
      if ((*i).isObject() && (*i).isMember("filename")) {
        for (auto app : config.docker_apps) {
          if (i.key().asString() == app && (*i)["filename"].asString() == t.filename()) {
            if (!cb(app, t)) {
              res = false;
            }
          }
        }
      } else {
        LOG_ERROR << "Invalid custom data for docker-app: " << i.key().asString() << " -> " << *i;
      }
    }
  }
  return res;
}

bool DockerAppManager::fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                                   FetcherProgressCb progress_cb, const api::FlowControlToken *token) {
  if (!OstreeManager::fetchTarget(target, fetcher, keys, progress_cb, token)) {
    return false;
  }

  LOG_INFO << "Looking for DockerApps to fetch";
  auto cb = [this, &fetcher, &keys, progress_cb, token](const std::string &app, const Uptane::Target &app_target) {
    LOG_INFO << "Fetching " << app << " -> " << app_target;
    return PackageManagerInterface::fetchTarget(app_target, fetcher, keys, progress_cb, token);
  };
  return iterate_apps(target, cb);
}
