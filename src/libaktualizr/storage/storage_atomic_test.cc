#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include <boost/filesystem.hpp>
#include <libaktualizr/utils.h>

#include "storage/sqlstorage.h"
#include "logging/logging.h"

StorageType storage_test_type;

std::unique_ptr<INvStorage> Storage(const StorageConfig& config) {
  if (config.type == StorageType::kSqlite) {
    return std::unique_ptr<INvStorage>(new SQLStorage(config, false));
  } else {
    throw std::runtime_error("Invalid config type");
  }
}

StorageConfig MakeConfig(StorageType type, const boost::filesystem::path& storage_dir) {
  StorageConfig config;

  config.type = type;
  if (config.type == StorageType::kSqlite) {
    config.sqldb_path = storage_dir / "test.db";
  } else {
    throw std::runtime_error("Invalid config type");
  }
  return config;
}

struct TightProcess {
  pid_t pid;
  int pipefd[2];
  TemporaryDirectory storage_dir;
};

static void tight_store_keys(const TightProcess& t) {
  StorageConfig config = MakeConfig(storage_test_type, t.storage_dir.Path());
  std::unique_ptr<INvStorage> storage = Storage(config);
  unsigned int k = 0;
  char c = 'r';

  EXPECT_EQ(write(t.pipefd[1], &c, 1), 1);
  while (true) {
    storage->storePrimaryKeys(std::to_string(k), std::to_string(k));
    k += 1;
  }
}

static void check_consistent_state(const boost::filesystem::path& storage_dir) {
  StorageConfig config = MakeConfig(storage_test_type, storage_dir);
  std::unique_ptr<INvStorage> storage = Storage(config);
  std::string pub, priv;

  EXPECT_TRUE(storage->loadPrimaryKeys(&pub, &priv));

  EXPECT_EQ(pub, priv);
}

void atomic_test() {
  unsigned int n_procs = 70;
  std::list<TightProcess> procs;

  for (auto k = 0u; k < n_procs; k++) {
    procs.emplace_back();
    TightProcess& p = procs.back();

    EXPECT_EQ(pipe(p.pipefd), 0);

    pid_t pid = fork();
    if (pid != 0) {
      close(p.pipefd[1]);
      p.pid = pid;

    } else {
      close(p.pipefd[0]);
      tight_store_keys(p);
      exit(0);
    }
  }

  for (const auto& p : procs) {
    // wait for readiness
    char c;
    EXPECT_EQ(read(p.pipefd[0], &c, 1), 1);
    close(p.pipefd[0]);
  }

  std::mt19937_64 eng{12};
  std::uniform_int_distribution<> dist{1, 30};
  while (!procs.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});

    TightProcess& p = procs.back();
    kill(p.pid, SIGKILL);
    waitpid(p.pid, NULL, 0);
    check_consistent_state(p.storage_dir.Path());

    procs.pop_back();
  }
}

// To run these tests:
// ./build/tests/t_storage_atomic --gtest_also_run_disabled_tests

TEST(DISABLED_storage_atomic, sql) {
  // disabled for now because it uses too much resources for CI
  storage_test_type = StorageType::kSqlite;
  atomic_test();
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
