#include "provision.h"
#include <config/config.h>
#include <primary/events.h>
#include <primary/sotauptaneclient.h>
#include <uptane/uptanerepository.h>
#include <http/httpclient.h>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "executor.h"

using namespace boost::filesystem;
using ptree = boost::property_tree::ptree;

path mkDeviceBaseDir(const boost::uuids::uuid &deviceId, const path &dstDir) {
  path deviceBaseDir{dstDir};
  deviceBaseDir /= to_string(deviceId);
  create_directory(deviceBaseDir);
  permissions(deviceBaseDir, remove_perms | group_write | others_write);
  return deviceBaseDir;
}

path writeDeviceConfig(const ptree &cfgTemplate, const path &deviceBaseDir, const boost::uuids::uuid &deviceId) {
  ptree deviceCfg{cfgTemplate};
  path cfgFilePath{deviceBaseDir};
  cfgFilePath /= "sota.toml";
  deviceCfg.put_child("storage.path", ptree("\"" + deviceBaseDir.native() + "\""));
  deviceCfg.put_child("storage.type", ptree("\"filesystem\""));
  deviceCfg.put_child("uptane.primary_ecu_serial", ptree("\"" + to_string(deviceId) + "\""));
  boost::property_tree::ini_parser::write_ini(cfgFilePath.native(), deviceCfg);
  return cfgFilePath;
}

class ProvisionDeviceTask {
  Config config;
  std::shared_ptr<INvStorage> storage;
  HttpClient httpClient;

 public:
  ProvisionDeviceTask(const path configFile)
      : config{configFile}, storage{INvStorage::newStorage(config.storage)}, httpClient{} {}

  void operator()() {
    Uptane::Repository repo{config, storage, httpClient};
    auto eventsIn = std::make_shared<event::Channel>();
    SotaUptaneClient client{config, eventsIn, repo, storage, httpClient};
    try {
      if (repo.initialize()) {
        repo.putManifest(client.AssembleManifest());
      } else {
        LOG_ERROR << "Failed to initialize repository for " << config.storage.path;
      }
    } catch (std::exception &err) {
      LOG_ERROR << "Failed to register device " << config.storage.path << ": " << err.what();
    } catch (...) {
      LOG_ERROR << "Failed to register device " << config.storage.path;
    }
  }
};

class ProvisionDeviceTaskStream {
  boost::uuids::basic_random_generator<boost::random::mt19937> gen;
  const path &dstDir;
  const ptree &cfgTemplate;

 public:
  ProvisionDeviceTaskStream(const path &dstDir_, const ptree &ct) : gen{}, dstDir{dstDir_}, cfgTemplate{ct} {}

  ProvisionDeviceTask nextTask() {
    LOG_INFO << "Creating provision device task";
    const boost::uuids::uuid deviceId = gen();
    const path deviceBaseDir = mkDeviceBaseDir(deviceId, dstDir);
    const path deviceCfgPath = writeDeviceConfig(cfgTemplate, deviceBaseDir, deviceId);
    return ProvisionDeviceTask(deviceCfgPath);
  }
};

void mkDevices(const path &dstDir, const path bootstrapCredentials, const std::string gw_uri, const size_t parallelism,
               const uint nr, const uint rate) {
  ptree cfgTemplate{};
  cfgTemplate.put_child("tls.server", ptree("\"https://" + gw_uri + "\""));
  cfgTemplate.put_child("provision.server", ptree("\"https://" + gw_uri + "\""));
  cfgTemplate.put_child("provision.provision_path", ptree("\"" + bootstrapCredentials.native() + "\""));
  // cfgTemplate.put_child("pacman.sysroot", ptree("\"/sysroot\""));
  cfgTemplate.put_child("pacman.type", ptree("\"none\""));
  std::vector<ProvisionDeviceTaskStream> feeds;
  for (size_t i = 0; i < parallelism; i++) {
    feeds.push_back(ProvisionDeviceTaskStream{dstDir, cfgTemplate});
  }
  FixedExecutionController execController{nr};
  Executor<ProvisionDeviceTaskStream> exec{feeds, rate, execController};
  exec.run();
}