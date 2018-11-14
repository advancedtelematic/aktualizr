#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include "fetcher.h"
#include "logging/logging.h"
#include "storage/sqlstorage.h"
#include "test_utils.h"
#include "tuf.h"

static std::string server = "http://127.0.0.1:";
static std::string treehub_server = "http://127.0.0.1:";
std::string sysroot;

unsigned int num_events_DownloadPause = 0;
void process_events_DownloadPauseResume(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "DownloadProgressReport") {
    return;
  }
  switch (num_events_DownloadPause) {
    case 0: {
      EXPECT_EQ(event->variant, "DownloadPaused");
      const auto pause_event = dynamic_cast<event::DownloadPaused*>(event.get());
      EXPECT_EQ(pause_event->result, PauseResult::kNotDownloading);
      break;
    }
    case 1: {
      EXPECT_EQ(event->variant, "DownloadPaused");
      const auto pause_event = dynamic_cast<event::DownloadPaused*>(event.get());
      EXPECT_EQ(pause_event->result, PauseResult::kPaused);
      break;
    }
    case 2: {
      EXPECT_EQ(event->variant, "DownloadPaused");
      const auto pause_event = dynamic_cast<event::DownloadPaused*>(event.get());
      EXPECT_EQ(pause_event->result, PauseResult::kAlreadyPaused);
      break;
    }
    case 3: {
      EXPECT_EQ(event->variant, "DownloadResumed");
      const auto resume_event = dynamic_cast<event::DownloadResumed*>(event.get());
      EXPECT_EQ(resume_event->result, PauseResult::kResumed);
      break;
    }
    case 4: {
      EXPECT_EQ(event->variant, "DownloadResumed");
      const auto resume_event = dynamic_cast<event::DownloadResumed*>(event.get());
      EXPECT_EQ(resume_event->result, PauseResult::kNotPaused);
      break;
    }
    default:
      std::cout << "event #" << num_events_DownloadPause << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  };
  ++num_events_DownloadPause;
}

/* Pause downloading.
 * Pausing while paused is ignored.
 * Pausing while not downloading is ignored.
 * Resume downloading.
 * Resuming while not paused is ignored. */
void test_pause(const Uptane::Target& target) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.uptane.repo_server = server;
  config.pacman.sysroot = sysroot;
  config.pacman.ostree_server = treehub_server;

  std::shared_ptr<INvStorage> storage(new SQLStorage(config.storage, false));
  auto http = std::make_shared<HttpClient>();
  auto events_channel = std::make_shared<event::Channel>();
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_DownloadPauseResume;
  events_channel->connect(f_cb);
  Uptane::Fetcher f(config, storage, http, events_channel);

  std::promise<void> end_pausing;

  EXPECT_EQ(f.setPause(true), PauseResult::kNotDownloading);

  std::thread([&f, &end_pausing] {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(f.setPause(true), PauseResult::kPaused);
    EXPECT_EQ(f.setPause(true), PauseResult::kAlreadyPaused);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(f.setPause(false), PauseResult::kResumed);
    EXPECT_EQ(f.setPause(false), PauseResult::kNotPaused);
    end_pausing.set_value();
  })
      .detach();

  auto start = std::chrono::high_resolution_clock::now();
  auto result = f.fetchVerifyTarget(target);
  auto finish = std::chrono::high_resolution_clock::now();

  auto status = end_pausing.get_future().wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
  }

  EXPECT_TRUE(result);
  EXPECT_EQ(num_events_DownloadPause, 5);
  EXPECT_GE((finish - start), std::chrono::seconds(2));
}

TEST(fetcher, test_pause_ostree) {
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "16ef2f2629dc9263fdf3c0f032563a2d757623bbc11cf99df25c3c3f258dccbe";
  target_json["custom"]["targetFormat"] = "OSTREE";
  target_json["length"] = 0;
  Uptane::Target target("pause", target_json);
  num_events_DownloadPause = 0;
  test_pause(target);
}

TEST(fetcher, test_pause_binary) {
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "d03b1a2081755f3a5429854cc3e700f8cbf125db2bd77098ae79a7d783256a7d";
  target_json["length"] = 2048;

  Uptane::Target target("large_interrupted", target_json);
  num_events_DownloadPause = 0;
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
  std::string treehub_port = TestUtils::getFreePort();
  treehub_server += treehub_port;
  TestHelperProcess ostree_server_process("tests/sota_tools/treehub_server.py", treehub_port);

  TemporaryDirectory temp_dir;
  int r = system((std::string("cp -r ") + argv[1] + std::string(" ") + temp_dir.PathString()).c_str());
  if (r != 0) {
    return -1;
  }
  sysroot = (temp_dir.Path() / "ostree_repo").string();
  TestUtils::waitForServer(server + "/");
  TestUtils::waitForServer(treehub_server + "/");
  return RUN_ALL_TESTS();
}
#endif
