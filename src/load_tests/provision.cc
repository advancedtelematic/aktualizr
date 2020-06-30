#include "provision.h"

#include <boost/property_tree/ini_parser.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "libaktualizr/aktualizr.h"

#include "libaktualizr/config.h"
#include "libaktualizr/events.h"

#include "context.h"
#include "executor.h"
#include "http/httpclient.h"
#include "logging/logging.h"
#include "primary/reportqueue.h"
#include "primary/sotauptaneclient.h"
#include "uptane/uptanerepository.h"
#include "utilities/utils.h"

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
  deviceCfg.put_child("storage.type", ptree("\"sqlite\""));
  deviceCfg.put_child("provision.primary_ecu_serial", ptree("\"" + to_string(deviceId) + "\""));
  boost::property_tree::ini_parser::write_ini(cfgFilePath.native(), deviceCfg);
  return cfgFilePath;
}

class ProvisionDeviceTask {
  Config config;
  std::unique_ptr<Aktualizr> aktualizr_ptr;

 public:
  explicit ProvisionDeviceTask(const Config cfg) : config{cfg}, aktualizr_ptr{std_::make_unique<Aktualizr>(config)} {
    logger_set_threshold(boost::log::trivial::severity_level::trace);
  }

  ProvisionDeviceTask(const ProvisionDeviceTask &) = delete;
  ProvisionDeviceTask(ProvisionDeviceTask &&) = default;

  void operator()() {
    try {
      aktualizr_ptr->Initialize();
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
    Config config = configure(deviceCfgPath, logLevel);
    return ProvisionDeviceTask{config};
  }
};

void mkDevices(const path &dstDir, const path &bootstrapCredentials, const std::string &gw_uri,
               const size_t parallelism, const unsigned int nr, const unsigned int rate) {
  const int severity = loggerGetSeverity();
  ptree cfgTemplate{};
  cfgTemplate.put_child("tls.server", ptree("\"https://" + gw_uri + "\""));
  cfgTemplate.put_child("provision.server", ptree("\"https://" + gw_uri + "\""));
  cfgTemplate.put_child("provision.provision_path", ptree("\"" + bootstrapCredentials.native() + "\""));
  // cfgTemplate.put_child("pacman.sysroot", ptree("\"/sysroot\""));
  cfgTemplate.put_child("pacman.type", ptree("\"none\""));
  std::vector<ProvisionDeviceTaskStream> feeds;
  for (size_t i = 0; i < parallelism; i++) {
    feeds.emplace_back(dstDir, cfgTemplate, severity);
  }
  std::unique_ptr<ExecutionController> execController = std_::make_unique<FixedExecutionController>(nr);
  Executor<ProvisionDeviceTaskStream> exec{feeds, rate, std::move(execController), "Provision"};
  exec.run();
}
