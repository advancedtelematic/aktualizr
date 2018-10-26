#include <gtest/gtest.h>

#include <unistd.h>
#include <memory>
#include <string>

#include <json/json.h>

#include "config/config.h"
#include "httpfake.h"
#include "reportqueue.h"
#include "utilities/types.h"  // TimeStamp
#include "utilities/utils.h"

/* Test one event. */
TEST(ReportQueue, SingleEvent) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "reportqueue/SingleEvent";

  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  ReportQueue report_queue(config, http);

  report_queue.enqueue(std_::make_unique<DownloadCompleteReport>("SingleEvent"));

  // Wait at most 30 seconds for the message to get processed.
  size_t counter = 0;
  size_t num_events = 1;
  while (http->events_seen < num_events) {
    sleep(1);
    ASSERT_LT(++counter, 30) << "Timed out waiting for event report.";
  }
  EXPECT_EQ(http->events_seen, num_events);
}

/* Test ten events. */
TEST(ReportQueue, MultipleEvents) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "reportqueue/MultipleEvents";

  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  ReportQueue report_queue(config, http);

  for (int i = 0; i < 10; ++i) {
    report_queue.enqueue(std_::make_unique<DownloadCompleteReport>("MultipleEvents" + std::to_string(i)));
  }

  // Wait at most 30 seconds for the messages to get processed.
  size_t counter = 0;
  size_t num_events = 10;
  while (http->events_seen < num_events) {
    sleep(1);
    ASSERT_LT(++counter, 30) << "Timed out waiting for event reports.";
  }
  EXPECT_EQ(http->events_seen, num_events);
}

/* Test ten events, but the "server" returns an error the first nine times. The
 * tenth time should succeed with an array of all ten events. */
TEST(ReportQueue, FailureRecovery) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.tls.server = "reportqueue/FailureRecovery";

  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  ReportQueue report_queue(config, http);

  for (int i = 0; i < 10; ++i) {
    report_queue.enqueue(std_::make_unique<DownloadCompleteReport>("FailureRecovery" + std::to_string(i)));
  }

  // Wait at most 30 seconds for the messages to get processed.
  size_t counter = 0;
  size_t num_events = 10;
  while (http->events_seen < num_events) {
    std::cout << "events_seen: " << http->events_seen << "\n";
    sleep(1);
    ASSERT_LT(++counter, 30) << "Timed out waiting for event reports.";
  }
  EXPECT_EQ(http->events_seen, num_events);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
