#include "check.h"
#include <random>
#include <string>
#include "context.h"
#include "executor.h"
#include "primary/events.h"
#include "primary/sotauptaneclient.h"
#include "storage/fsstorage.h"
#include "uptane/uptanerepository.h"

class EphemeralStorage : public FSStorage {
 public:
  EphemeralStorage(const StorageConfig &config) : FSStorage(config) {}
  void storeMetadata(const Uptane::RawMetaPack &) override{};

  static std::shared_ptr<INvStorage> newStorage(const StorageConfig &config) {
    return std::make_shared<EphemeralStorage>(config);
  }
};

class CheckForUpdate {
  Config config;

  std::shared_ptr<INvStorage> storage;

  HttpClient httpClient;

 public:
  CheckForUpdate(Config config_)
      : config{config_}, storage{EphemeralStorage::newStorage(config.storage)}, httpClient{} {}

  void operator()() {
    LOG_DEBUG << "Updating a device in " << config.storage.path.native();
    Uptane::Repository repo{config, storage};
    auto eventsIn = std::make_shared<event::Channel>();
    Bootloader bootloader(config.bootloader);
    SotaUptaneClient client{config, eventsIn, repo, storage, httpClient, bootloader};
    try {
      std::string pkey;
      std::string cert;
      std::string ca;
      if (storage->loadTlsCreds(&ca, &cert, &pkey)) {
        httpClient.setCerts(ca, CryptoSource::File, cert, CryptoSource::File, pkey, CryptoSource::File);
        LOG_DEBUG << "Getting targets";
        client.getUpdateRequests();
      } else {
        LOG_ERROR << "Unable to load device's credentials";
      }
      /* repo.authenticate(); */
    } catch (const Uptane::MissingRepo &e) {
      LOG_DEBUG << e.what();
    } catch (const std::exception &e) {
      LOG_ERROR << "Unable to get new targets: " << e.what();
    } catch (...) {
      LOG_ERROR << "Unknown error occured while checking for updates";
    }
  }
};

using namespace boost::random;

class CheckForUpdateTasks {
  std::vector<Config> configs;

  mt19937 rng;

  uniform_int_distribution<> gen;

 public:
  CheckForUpdateTasks(const boost::filesystem::path baseDir)
      : configs{loadDeviceConfigurations(baseDir)}, gen(0, configs.size() - 1) {
    std::random_device seedGen;
    rng.seed(seedGen());
  }

  CheckForUpdate nextTask() { return CheckForUpdate{configs[gen(rng)]}; }
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
