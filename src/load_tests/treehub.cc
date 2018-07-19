#include "treehub.h"
#include <gio/gio.h>
#include <glib.h>
#include <ostree.h>
#include <package_manager/ostreemanager.h>
#include <random>
#include <vector>
#include "context.h"
#include "executor.h"

namespace fs = boost::filesystem;

class FetchTask {
  const Config &config;
  std::shared_ptr<INvStorage> storage;
  std::shared_ptr<KeyManager> keys;
  const fs::path repoDir;
  const std::string branchName;
  const std::string remoteUrl;

  OstreeRepoPtr repo;

  void initRepo() {
    GFile *gRepoFile = g_file_new_for_path(repoDir.native().c_str());
    repo.reset(ostree_repo_new(gRepoFile));
    g_object_unref(gRepoFile);
    if (!repo) {
      throw std::runtime_error("Repo initialization failed");
    }

    if (!ostree_repo_create(repo.get(), OSTREE_REPO_MODE_ARCHIVE_Z2, NULL, NULL)) {
      throw std::runtime_error("Unable to init repository");
    };

    if (!OstreeManager::addRemote(repo.get(), remoteUrl, *keys)) {
      throw std::runtime_error("Unable to add remote to the repository");
    };
  }

 public:
  FetchTask(const Config &cfg, const fs::path rd, const std::string &bn, const std::string &rurl)
      : config{cfg},
        storage{INvStorage::newStorage(config.storage)},
        keys{new KeyManager{storage, config.keymanagerConfig()}},
        repoDir{rd},
        branchName{bn},
        remoteUrl{rurl} {
    keys->loadKeys();
    initRepo();
  }

  void operator()() {
    GVariantBuilder builder;
    GVariant *options;
    const char *const refs[] = {branchName.c_str()};
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{s@v}", "flags", g_variant_new_variant(g_variant_new_int32(0)));
    g_variant_builder_add(&builder, "{s@v}", "refs", g_variant_new_variant(g_variant_new_strv(refs, 1)));
    options = g_variant_builder_end(&builder);
    GError *error = NULL;
    if (!ostree_repo_pull_with_options(repo.get(), remote, options, NULL, NULL, &error)) {
      LOG_ERROR << "Failed to pull repo " << repoDir << ": " << error->message;
      g_error_free(error);
      error = NULL;
    }
    g_variant_unref(options);
  }
};

class FetchFromOstreeTasks {
  std::vector<Config> configs;
  const fs::path outputDir;
  const std::string &branchName;
  const std::string &remoteUrl;
  std::mt19937 rng;
  std::uniform_int_distribution<size_t> gen;

 public:
  FetchFromOstreeTasks(const fs::path &baseDir, const fs::path &od, const std::string &bn, const std::string &ru)
      : configs{loadDeviceConfigurations(baseDir)},
        outputDir{od},
        branchName{bn},
        remoteUrl{ru},
        gen(0UL, configs.size() - 1) {
    std::random_device seedGen;
    rng.seed(seedGen());
  }

  FetchTask nextTask() {
    auto dstName = Utils::randomUuid();
    const fs::path repoDir = outputDir / dstName;
    return FetchTask{configs[gen(rng)], repoDir, branchName, remoteUrl};
  }
};

void fetchFromOstree(const fs::path &baseDir, const fs::path &outputDir, const std::string &branchName,
                     const std::string &remoteUrl, const unsigned int rate, const unsigned int nr,
                     const unsigned int parallelism) {
  std::vector<FetchFromOstreeTasks> feeds(parallelism, FetchFromOstreeTasks{baseDir, outputDir, branchName, remoteUrl});
  std::unique_ptr<FixedExecutionController> execController = std_::make_unique<FixedExecutionController>(nr);
  Executor<FetchFromOstreeTasks> exec{feeds, rate, std::move(execController), "Fetch"};
  exec.run();
}
