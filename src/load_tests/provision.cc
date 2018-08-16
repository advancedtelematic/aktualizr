#include "provision.h"

#include <boost/property_tree/ini_parser.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "config/config.h"
#include "context.h"
#include "executor.h"
#include "http/httpclient.h"
#include "logging/logging.h"
#include "primary/reportqueue.h"
#include "primary/sotauptaneclient.h"
#include "uptane/uptanerepository.h"
#include "utilities/events.h"

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
  std::shared_ptr<HttpClient> httpClient;

 public:
  ProvisionDeviceTask(const Config cfg)
      : config{cfg}, storage{INvStorage::newStorage(config.storage)}, httpClient{std::make_shared<HttpClient>()} {
    logger_set_threshold(boost::log::trivial::severity_level::trace);
  }

  void operator()() {
    Uptane::Manifest manifest{config, storage};
    auto client = SotaUptaneClient::newTestClient(config, storage, httpClient);
    try {
      if (client->initialize()) {
        auto signed_manifest = manifest.signManifest(client->AssembleManifest());
        httpClient->put(config.uptane.director_server + "/manifest", signed_manifest);
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
  const int logLevel;

 public:
  ProvisionDeviceTaskStream(const path &dstDir_, const ptree &ct, const int ll)
      : gen{}, dstDir{dstDir_}, cfgTemplate{ct}, logLevel{ll} {}

  ProvisionDeviceTask nextTask() {
    LOG_INFO << "Creating provision device task";
    const boost::uuids::uuid deviceId = gen();
    const path deviceBaseDir = mkDeviceBaseDir(deviceId, dstDir);
    const path deviceCfgPath = writeDeviceConfig(cfgTemplate, deviceBaseDir, deviceId);
    return ProvisionDeviceTask(configure(deviceCfgPath, logLevel));
  }
};

void mkDevices(const path &dstDir, const path bootstrapCredentials, const std::string gw_uri, const size_t parallelism,
               const unsigned int nr, const unsigned int rate) {
  const int severity = loggerGetSeverity();
  ptree cfgTemplate{};
  cfgTemplate.put_child("tls.server", ptree("\"https://" + gw_uri + "\""));
  cfgTemplate.put_child("provision.server", ptree("\"https://" + gw_uri + "\""));
  cfgTemplate.put_child("provision.provision_path", ptree("\"" + bootstrapCredentials.native() + "\""));
  // cfgTemplate.put_child("pacman.sysroot", ptree("\"/sysroot\""));
  cfgTemplate.put_child("pacman.type", ptree("\"none\""));
  std::vector<ProvisionDeviceTaskStream> feeds;
  for (size_t i = 0; i < parallelism; i++) {
    feeds.push_back(ProvisionDeviceTaskStream{dstDir, cfgTemplate, severity});
  }
  std::unique_ptr<ExecutionController> execController = std_::make_unique<FixedExecutionController>(nr);
  Executor<ProvisionDeviceTaskStream> exec{feeds, rate, std::move(execController), "Provision"};
  exec.run();
}
