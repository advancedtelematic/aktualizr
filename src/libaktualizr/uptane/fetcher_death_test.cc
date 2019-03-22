#include <gtest/gtest.h>

#include <boost/process.hpp>
#include <chrono>
#include <csignal>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include "fetcher.h"
#include "http/httpclient.h"
#include "logging/logging.h"
#include "storage/sqlstorage.h"
#include "test_utils.h"
#include "tuf.h"

static const int die_after = 50;       // percent
static const int pause_duration = 20;  // seconds

std::string server;

static std::mutex pause_m;
static std::condition_variable cv;
static bool die = false;
static bool resumed = false;

Config config;

static void progress_cb(const Uptane::Target& target, const std::string& description, unsigned int progress) {
  (void)target;
  (void)description;
  std::cout << "progress: " << progress << std::endl;
  if (progress >= die_after) {
    std::lock_guard<std::mutex> lk(pause_m);
    die = true;
    cv.notify_all();
  }
  if (resumed) {
    EXPECT_GE(progress, die_after);
  }
}

void resume(const Uptane::Target& target) {
  std::shared_ptr<INvStorage> storage(new SQLStorage(config.storage, false));
  auto http = std::make_shared<HttpClient>();
  Uptane::Fetcher f(config, storage, http, progress_cb);

  resumed = true;
  bool res = f.fetchVerifyTarget(target);

  EXPECT_TRUE(res);
}

void pause_and_die(const Uptane::Target& target) {
  std::shared_ptr<INvStorage> storage(new SQLStorage(config.storage, false));
  auto http = std::make_shared<HttpClient>();
  Uptane::Fetcher f(config, storage, http, progress_cb);
  api::FlowControlToken token;

  std::promise<bool> download_promise;
  auto result = download_promise.get_future();

  std::thread([&f, &target, &download_promise, &token]() {
    bool res = f.fetchVerifyTarget(target, &token);
    download_promise.set_value(res);
  })
      .detach();

  std::unique_lock<std::mutex> lk(pause_m);
  cv.wait(lk, [] { return die; });
  token.setPause(true);
  std::this_thread::sleep_for(std::chrono::seconds(pause_duration));
  std::raise(SIGKILL);
}

TEST(FetcherDeathTest, TestResumeBinary) {
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "dd7bd1c37a3226e520b8d6939c30991b1c08772d5dab62b381c3a63541dc629a";
  target_json["length"] = 100 * (1 << 20);

  Uptane::Target target("large_file", target_json);
  ASSERT_DEATH(pause_and_die(target), "");
  std::cout << "Fetcher died, retrying" << std::endl;
  resume(target);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  std::string port = TestUtils::getFreePort();
  server = "http://127.0.0.1:" + port;
  config.uptane.repo_server = server;
  boost::process::child http_server_process("tests/fake_http_server/fake_http_server.py", port);
  TestUtils::waitForServer(server + "/");
  return RUN_ALL_TESTS();
}
#endif  // __NO_MAIN__
