#include "check.h"

#include <random>
#include <string>

#include "context.h"
#include "executor.h"
#include "primary/reportqueue.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "storage/sqlstorage.h"
#include "uptane/uptanerepository.h"
#include "utilities/events.h"

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

  std::shared_ptr<INvStorage> storage;

  std::shared_ptr<HttpClient> httpClient;

 public:
  CheckForUpdate(Config config_)
      : config{config_},
        storage{EphemeralStorage::newStorage(config.storage)},
        httpClient{std::make_shared<HttpClient>()} {}

  void operator()() {
    LOG_DEBUG << "Updating a device in " << config.storage.path.native();
    auto client = SotaUptaneClient::newTestClient(config, storage, httpClient);
    try {
      std::string pkey;
      std::string cert;
      std::string ca;
      if (storage->loadTlsCreds(&ca, &cert, &pkey)) {
        httpClient->setCerts(ca, CryptoSource::kFile, cert, CryptoSource::kFile, pkey, CryptoSource::kFile);
        LOG_DEBUG << "Getting targets";
        client->updateMeta();
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
