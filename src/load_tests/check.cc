#include "check.h"

#include <libaktualizr/aktualizr.h>
#include <random>
#include <string>
#include <libaktualizr/utils.h>

#include "context.h"
#include "executor.h"
#include <libaktualizr/events.h>
#include "primary/reportqueue.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "storage/sqlstorage.h"
#include "uptane/uptanerepository.h"

namespace fs = boost::filesystem;

class EphemeralStorage : public SQLStorage {
 public:
  EphemeralStorage(const StorageConfig &config, bool readonly) : SQLStorage(config, readonly) {}
  void storeRoot(const std::string &data, Uptane::RepositoryType repo, Uptane::Version version) override {
    (void)data;
    (void)repo;
    (void)version;
  };
  void storeNonRoot(const std::string &data, Uptane::RepositoryType repo, Uptane::Role role) override {
    (void)data;
    (void)repo;
    (void)role;
  };

  static std::shared_ptr<INvStorage> newStorage(const StorageConfig &config) {
    return std::make_shared<EphemeralStorage>(config, false);
  }
};

class CheckForUpdate {
  Config config;

  std::unique_ptr<Aktualizr> aktualizr_ptr;

 public:
  CheckForUpdate(Config config_) : config{config_}, aktualizr_ptr{std_::make_unique<Aktualizr>(config)} {
    try {
      aktualizr_ptr->Initialize();
    } catch (...) {
      LOG_ERROR << "Unable to initialize a device: " << config.storage.path;
    }
  }

  void operator()() {
    LOG_DEBUG << "Updating a device in " << config.storage.path.native();
    try {
      aktualizr_ptr->CheckUpdates().get();
    } catch (const Uptane::MissingRepo &e) {
      LOG_DEBUG << e.what();
    } catch (const std::exception &e) {
      LOG_ERROR << "Unable to get new targets: " << e.what();
    } catch (...) {
      LOG_ERROR << "Unknown error occured while checking for updates";
    }
  }
};

class CheckForUpdateTasks {
  std::vector<Config> configs;

  std::mt19937 rng;

  std::uniform_int_distribution<size_t> gen;

 public:
  CheckForUpdateTasks(const boost::filesystem::path baseDir)
      : configs{loadDeviceConfigurations(baseDir)}, gen(0UL, configs.size() - 1) {
    std::random_device seedGen;
    rng.seed(seedGen());
  }

  CheckForUpdate nextTask() {
    auto srcConfig = configs[gen(rng)];
    auto config{srcConfig};
    config.storage.path = fs::temp_directory_path() / fs::unique_path();
    LOG_DEBUG << "Copy device " << srcConfig.storage.path << " into " << config.storage.path;
    fs::create_directory(config.storage.path);
    fs::permissions(config.storage.path, fs::remove_perms | fs::group_write | fs::others_write);
    fs::copy_file(srcConfig.storage.sqldb_path.get(srcConfig.storage.path),
                  config.storage.sqldb_path.get(config.storage.path));
    return CheckForUpdate{config};
  }
};

void checkForUpdates(const boost::filesystem::path &baseDir, const unsigned int rate, const unsigned int nr,
                     const unsigned int parallelism) {
  LOG_INFO << "Target rate: " << rate << "op/s, operations: " << nr << ", workers: " << parallelism;
  std::vector<CheckForUpdateTasks> feeds(parallelism, CheckForUpdateTasks{baseDir});
  std::unique_ptr<ExecutionController> execController;
  if (nr == 0) {
    execController = std_::make_unique<InterruptableExecutionController>();
  } else {
    execController = std_::make_unique<FixedExecutionController>(nr);
  }
  Executor<CheckForUpdateTasks> exec{feeds, rate, std::move(execController), "Check for updates"};
  exec.run();
}
