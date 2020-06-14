#include <gtest/gtest.h>

#include <unistd.h>
#include <future>
#include <memory>
#include <string>

#include <json/json.h>

#include <libaktualizr/config.h>
#include <libaktualizr/types.h>  // TimeStamp
#include <libaktualizr/utils.h>

#include "httpfake.h"
#include "reportqueue.h"
#include "storage/invstorage.h"
#include "storage/sqlstorage.h"

class HttpFakeRq : public HttpFake {
 public:
  HttpFakeRq(const boost::filesystem::path &test_dir_in, size_t expected_events)
      : HttpFake(test_dir_in, ""), expected_events_(expected_events) {}

  HttpResponse handle_event(const std::string &url, const Json::Value &data) override {
    (void)data;
    if (url == "reportqueue/SingleEvent/events") {
      EXPECT_EQ(data[0]["eventType"]["id"], "EcuDownloadCompleted");
      EXPECT_EQ(data[0]["event"]["ecu"], "SingleEvent");
      ++events_seen;
      if (events_seen == expected_events_) {
        expected_events_received.set_value(true);
      }
      return HttpResponse("", 200, CURLE_OK, "");
    } else if (url.find("reportqueue/MultipleEvents") == 0) {
      for (int i = 0; i < static_cast<int>(data.size()); ++i) {
        EXPECT_EQ(data[i]["eventType"]["id"], "EcuDownloadCompleted");
        EXPECT_EQ(data[i]["event"]["ecu"], "MultipleEvents" + std::to_string(events_seen++));
      }
      if (events_seen == expected_events_) {
        expected_events_received.set_value(true);
      }
      return HttpResponse("", 200, CURLE_OK, "");
    } else if (url.find("reportqueue/FailureRecovery") == 0) {
      if (data.size() < 10) {
        return HttpResponse("", 400, CURLE_OK, "");
      } else {
        for (int i = 0; i < static_cast<int>(data.size()); ++i) {
          EXPECT_EQ(data[i]["eventType"]["id"], "EcuDownloadCompleted");
          EXPECT_EQ(data[i]["event"]["ecu"], "FailureRecovery" + std::to_string(i));
        }
        events_seen = data.size();
        if (events_seen == expected_events_) {
          expected_events_received.set_value(true);
        }
        return HttpResponse("", 200, CURLE_OK, "");
      }
    } else if (url.find("reportqueue/StoreEvents") == 0) {
      for (int i = 0; i < static_cast<int>(data.size()); ++i) {
        EXPECT_EQ(data[i]["eventType"]["id"], "EcuDownloadCompleted");
        EXPECT_EQ(data[i]["event"]["ecu"], "StoreEvents" + std::to_string(events_seen++));
      }
      if (events_seen == expected_events_) {
        expected_events_received.set_value(true);
      }
      return HttpResponse("", 200, CURLE_OK, "");
    }
    LOG_ERROR << "Unexpected event: " << data;
    return HttpResponse("", 400, CURLE_OK, "");
  }

  size_t events_seen{0};
  size_t expected_events_;
  std::promise<bool> expected_events_received{};
};

/* Test one event. */
TEST(ReportQueue, SingleEvent) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "reportqueue/SingleEvent";

  size_t num_events = 1;
  auto http = std::make_shared<HttpFakeRq>(temp_dir.Path(), num_events);
  auto sql_storage = std::make_shared<SQLStorage>(config.storage, false);
  ReportQueue report_queue(config, http, sql_storage);

  report_queue.enqueue(std_::make_unique<EcuDownloadCompletedReport>(Uptane::EcuSerial("SingleEvent"), "", true));

  // Wait at most 30 seconds for the message to get processed.
  http->expected_events_received.get_future().wait_for(std::chrono::seconds(20));
  EXPECT_EQ(http->events_seen, num_events);
}

/* Test ten events. */
TEST(ReportQueue, MultipleEvents) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "reportqueue/MultipleEvents";

  size_t num_events = 10;
  auto http = std::make_shared<HttpFakeRq>(temp_dir.Path(), num_events);
  auto sql_storage = std::make_shared<SQLStorage>(config.storage, false);
  ReportQueue report_queue(config, http, sql_storage);

  for (int i = 0; i < 10; ++i) {
    report_queue.enqueue(std_::make_unique<EcuDownloadCompletedReport>(
        Uptane::EcuSerial("MultipleEvents" + std::to_string(i)), "", true));
  }

  // Wait at most 30 seconds for the messages to get processed.
  http->expected_events_received.get_future().wait_for(std::chrono::seconds(20));
  EXPECT_EQ(http->events_seen, num_events);
}

/* Test ten events, but the "server" returns an error the first nine times. The
 * tenth time should succeed with an array of all ten events. */
TEST(ReportQueue, FailureRecovery) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "reportqueue/FailureRecovery";

  size_t num_events = 10;
  auto http = std::make_shared<HttpFakeRq>(temp_dir.Path(), num_events);
  auto sql_storage = std::make_shared<SQLStorage>(config.storage, false);
  ReportQueue report_queue(config, http, sql_storage);

  for (size_t i = 0; i < num_events; ++i) {
    report_queue.enqueue(std_::make_unique<EcuDownloadCompletedReport>(
        Uptane::EcuSerial("FailureRecovery" + std::to_string(i)), "", true));
  }

  // Wait at most 20 seconds for the messages to get processed.
  http->expected_events_received.get_future().wait_for(std::chrono::seconds(20));
  EXPECT_EQ(http->events_seen, num_events);
}

/* Test persistent storage of unsent events in the database across
 * ReportQueue instantiations. */
TEST(ReportQueue, StoreEvents) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "";

  auto sql_storage = std::make_shared<SQLStorage>(config.storage, false);
  size_t num_events = 10;
  auto check_sql = [sql_storage](size_t count) {
    int64_t max_id = 0;
    Json::Value report_array{Json::arrayValue};
    sql_storage->loadReportEvents(&report_array, &max_id);
    EXPECT_EQ(max_id, count);
  };

  {
    auto http = std::make_shared<HttpFakeRq>(temp_dir.Path(), num_events);
    ReportQueue report_queue(config, http, sql_storage);
    for (size_t i = 0; i < num_events; ++i) {
      report_queue.enqueue(std_::make_unique<EcuDownloadCompletedReport>(
          Uptane::EcuSerial("StoreEvents" + std::to_string(i)), "", true));
    }
    check_sql(num_events);
  }

  config.tls.server = "reportqueue/StoreEvents";
  auto http = std::make_shared<HttpFakeRq>(temp_dir.Path(), num_events);
  ReportQueue report_queue(config, http, sql_storage);
  // Wait at most 20 seconds for the messages to get processed.
  http->expected_events_received.get_future().wait_for(std::chrono::seconds(20));
  EXPECT_EQ(http->events_seen, num_events);
  sleep(1);
  check_sql(0);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
