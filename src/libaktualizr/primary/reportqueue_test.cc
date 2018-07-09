#include <gtest/gtest.h>

#include <unistd.h>
#include <memory>
#include <string>

#include <json/json.h>

#include "config/config.h"
#include "httpfake.h"
#include "reportqueue.h"
#include "uptane/tuf.h"  // TimeStamp
#include "utilities/utils.h"

std::unique_ptr<Json::Value> makeEvent(const std::string& name) {
  auto report = std_::make_unique<Json::Value>();
  (*report)["id"] = Utils::randomUuid();
  (*report)["deviceTime"] = Uptane::TimeStamp::Now().ToString();
  (*report)["eventType"]["id"] = "DownloadComplete";
  (*report)["eventType"]["version"] = 1;
  (*report)["event"] = name;
  return report;
}

/* Test one event. */
TEST(ReportQueue, SingleEvent) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "reportqueue/SingleEvent";

  HttpFake http(temp_dir.Path());
  ReportQueue report_queue(config, http);

  report_queue.enqueue(makeEvent("SingleEvent"));

  // Wait at most 30 seconds for the message to get processed.
  int counter = 0;
  int num_events = 1;
  while (http.events_seen < num_events) {
    sleep(1);
    ASSERT_LT(++counter, 30);
  }
  EXPECT_EQ(http.events_seen, num_events);
}

/* Test ten events. */
TEST(ReportQueue, MultipleEvents) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "reportqueue/MultipleEvents";

  HttpFake http(temp_dir.Path());
  ReportQueue report_queue(config, http);

  for (int i = 0; i < 10; ++i) {
    report_queue.enqueue(makeEvent("MultipleEvents" + std::to_string(i)));
  }

  // Wait at most 30 seconds for the messages to get processed.
  int counter = 0;
  int num_events = 10;
  while (http.events_seen < num_events) {
    sleep(1);
    ASSERT_LT(++counter, 30);
  }
  EXPECT_EQ(http.events_seen, num_events);
}

/* Test ten events, but the "server" returns an error the first nine times. The
 * tenth time should succeed with an array of all ten events. */
TEST(ReportQueue, FailureRecovery) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "reportqueue/FailureRecovery";

  HttpFake http(temp_dir.Path());
  ReportQueue report_queue(config, http);

  for (int i = 0; i < 10; ++i) {
    report_queue.enqueue(makeEvent("FailureRecovery" + std::to_string(i)));
  }

  // Wait at most 30 seconds for the messages to get processed.
  int counter = 0;
  int num_events = 10;
  while (http.events_seen < num_events) {
    sleep(1);
    ASSERT_LT(++counter, 30);
  }
  EXPECT_EQ(http.events_seen, num_events);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
