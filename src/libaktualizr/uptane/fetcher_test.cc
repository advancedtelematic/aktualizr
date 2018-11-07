#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include "fetcher.h"
#include "logging/logging.h"
#include "storage/sqlstorage.h"
#include "test_utils.h"
#include "tuf.h"

static std::string server = "http://127.0.0.1:";
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

TEST(fetcher, fetch_with_pause) {
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "d03b1a2081755f3a5429854cc3e700f8cbf125db2bd77098ae79a7d783256a7d";
  target_json["length"] = 2048;

  Uptane::Target target("large_file", target_json);
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.uptane.repo_server = server;

  std::shared_ptr<INvStorage> storage(new SQLStorage(config.storage, false));
  auto http = std::make_shared<HttpClient>();
  auto events_channel = std::make_shared<event::Channel>();
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_DownloadPauseResume;
  events_channel->connect(f_cb);
  Uptane::Fetcher f(config, storage, http, events_channel);

  std::thread([&f] {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(f.setPause(true), PauseResult::kPaused);
    EXPECT_EQ(f.setPause(true), PauseResult::kAlreadyPaused);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(f.setPause(false), PauseResult::kResumed);
    EXPECT_EQ(f.setPause(false), PauseResult::kNotPaused);
  })
      .detach();

  EXPECT_EQ(f.setPause(true), PauseResult::kNotDownloading);
  auto start = std::chrono::high_resolution_clock::now();
  auto result = f.fetchVerifyTarget(target);
  auto finish = std::chrono::high_resolution_clock::now();

  EXPECT_TRUE(result);
  EXPECT_EQ(num_events_DownloadPause, 5);
  EXPECT_GE((finish - start), std::chrono::seconds(2));
}

TEST(fetcher, fetch_restore) {
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "d03b1a2081755f3a5429854cc3e700f8cbf125db2bd77098ae79a7d783256a7d";
  target_json["length"] = 2048;

  Uptane::Target target("large_interrupted", target_json);
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.uptane.repo_server = server;

  auto http = std::make_shared<HttpClient>();
  std::shared_ptr<INvStorage> storage(new SQLStorage(config.storage, false));

  {
    Uptane::Fetcher f(config, storage, http);
    auto result = f.fetchVerifyTarget(target);
    EXPECT_FALSE(result);

    auto h = storage->openTargetFile(target);
    EXPECT_TRUE(h->isPartial());
    EXPECT_TRUE(h->rsize() < 2048);
  }

  Uptane::Fetcher f(config, storage, http);
  auto result = f.fetchVerifyTarget(target);
  EXPECT_TRUE(result);
  auto h2 = storage->openTargetFile(target);
  EXPECT_FALSE(h2->isPartial());
  EXPECT_EQ(h2->rsize(), 2048);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  std::string port = TestUtils::getFreePort();
  server += port;
  TestHelperProcess server_process("tests/fake_http_server/fake_http_server.py", port);

  sleep(3);

  return RUN_ALL_TESTS();
}
#endif
