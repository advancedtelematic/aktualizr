#include "dockerappmanager.h"
#include "packagemanagerfactory.h"

AUTO_REGISTER_PACKAGE_MANAGER(PACKAGE_MANAGER_OSTREEDOCKERAPP, DockerAppManager);

DockerAppManagerConfig::DockerAppManagerConfig(const PackageConfig &pconfig) {
  const std::map<std::string, std::string> raw = pconfig.extra;

  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  if (raw.count("docker_apps") == 1) {
    std::string val = raw.at("docker_apps");
    if (val.length() > 0) {
      // token_compress_on allows lists like: "foo,bar", "foo, bar", or "foo bar"
      boost::split(docker_apps, val, boost::is_any_of(", "), boost::token_compress_on);
    }
  }
  if (raw.count("docker_apps_root") == 1) {
    docker_apps_root = raw.at("docker_apps_root");
  }
  if (raw.count("docker_app_params") == 1) {
    docker_app_params = raw.at("docker_app_params");
  }
  if (raw.count("docker_app_bin") == 1) {
    docker_app_bin = raw.at("docker_app_bin");
  }
  if (raw.count("docker_compose_bin") == 1) {
    docker_compose_bin = raw.at("docker_compose_bin");
  }

  if (raw.count("docker_prune") == 1) {
    std::string val = raw.at("docker_prune");
    boost::algorithm::to_lower(val);
    docker_prune = val != "0" && val != "false";
  }
}
