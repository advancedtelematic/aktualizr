#include <gtest/gtest.h>

#include <chrono>
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

static const int pause_after = 50;        // percent
static const int pause_duration = 1;      // seconds
static const int download_timeout = 200;  // seconds

static std::string server = "http://127.0.0.1:";
static std::string treehub_server = "http://127.0.0.1:";
std::string sysroot;

static std::mutex pause_m;
static std::condition_variable cv;
static bool do_pause = false;

Config config;

static void progress_cb(const Uptane::Target& target, const std::string& description, unsigned int progress) {
  (void)description;
  std::cout << "progress callback: " << progress << std::endl;
  if (!target.IsOstree() && !do_pause) {
    if (progress >= pause_after) {
      std::lock_guard<std::mutex> lk(pause_m);
      do_pause = true;
      cv.notify_all();
    }
  }
}

/* Pause downloading.
 * Pausing while paused is ignored.
 * Pausing while not downloading is ignored.
 * Resume downloading.
 * Resuming while not paused is ignored.
 * Resuming while not downloading is ignored
 */
void test_pause(const Uptane::Target& target) {
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  config.uptane.repo_server = server;
  config.pacman.sysroot = sysroot;
  config.pacman.ostree_server = treehub_server;

  std::shared_ptr<INvStorage> storage(new SQLStorage(config.storage, false));
  auto http = std::make_shared<HttpClient>();
  Uptane::Fetcher f(config, storage, http, progress_cb);
  EXPECT_EQ(f.setPause(true), Uptane::Fetcher::PauseRet::kNotDownloading);
  EXPECT_EQ(f.setPause(false), Uptane::Fetcher::PauseRet::kNotPaused);

  std::promise<void> pause_promise;
  std::promise<bool> download_promise;
  auto result = download_promise.get_future();
  auto pause_res = pause_promise.get_future();
  auto start = std::chrono::high_resolution_clock::now();

  std::thread([&f, &target, &download_promise]() {
    bool res = f.fetchVerifyTarget(target);
    download_promise.set_value(res);
  })
      .detach();

  std::thread([&f, &target, &pause_promise]() {
    if (!target.IsOstree()) {
      std::unique_lock<std::mutex> lk(pause_m);
      cv.wait(lk, [] { return do_pause; });
    } else {
      while (!f.isDownloading()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // wait for download start
      }
    }
    EXPECT_EQ(f.setPause(true), Uptane::Fetcher::PauseRet::kPaused);
    EXPECT_EQ(f.setPause(true), Uptane::Fetcher::PauseRet::kAlreadyPaused);
    std::this_thread::sleep_for(std::chrono::seconds(pause_duration));
    EXPECT_EQ(f.setPause(false), Uptane::Fetcher::PauseRet::kResumed);
    EXPECT_EQ(f.setPause(false), Uptane::Fetcher::PauseRet::kNotPaused);
    pause_promise.set_value();
  })
      .detach();

  ASSERT_EQ(result.wait_for(std::chrono::seconds(download_timeout)), std::future_status::ready);
  ASSERT_EQ(pause_res.wait_for(std::chrono::seconds(0)), std::future_status::ready);

  auto duration =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count();
  std::cout << "Downloaded 100MB in " << duration - pause_duration << "seconds\n";
  EXPECT_TRUE(result.get());
  EXPECT_GE(duration, pause_duration);
}

#ifdef BUILD_OSTREE
/*
 * Download an OSTree package
 * Verify an OSTree package
 */
TEST(fetcher, test_pause_ostree) {
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "16ef2f2629dc9263fdf3c0f032563a2d757623bbc11cf99df25c3c3f258dccbe";
  target_json["custom"]["targetFormat"] = "OSTREE";
  target_json["length"] = 0;
  Uptane::Target target("pause", target_json);
  test_pause(target);
}
#endif  // BUILD_OSTREE

TEST(fetcher, test_pause_binary) {
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "dd7bd1c37a3226e520b8d6939c30991b1c08772d5dab62b381c3a63541dc629a";
  target_json["length"] = 100 * (1 << 20);

  Uptane::Target target("large_file", target_json);
  test_pause(target);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  std::string port = TestUtils::getFreePort();
  server += port;
  TestHelperProcess http_server_process("tests/fake_http_server/fake_http_server.py", port);
  TestUtils::waitForServer(server + "/");
#ifdef BUILD_OSTREE
  std::string treehub_port = TestUtils::getFreePort();
  treehub_server += treehub_port;
  TestHelperProcess ostree_server_process("tests/sota_tools/treehub_server.py", treehub_port);

  TemporaryDirectory temp_dir;
  // Utils::copyDir doesn't work here. Complaints about non existent symlink path
  int r = system((std::string("cp -r ") + argv[1] + std::string(" ") + temp_dir.PathString()).c_str());
  if (r != 0) {
    return -1;
  }
  sysroot = (temp_dir.Path() / "ostree_repo").string();
  TestUtils::waitForServer(treehub_server + "/");
#endif  // BUILD_OSTREE
  return RUN_ALL_TESTS();
}
#endif  // __NO_MAIN__
