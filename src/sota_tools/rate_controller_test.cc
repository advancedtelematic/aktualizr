#include <gtest/gtest.h>

#include "rate_controller.h"

/* Initial rate controller status is good. */
TEST(initial, initially_ok) {
  RateController dut;
  EXPECT_FALSE(dut.ServerHasFailed());
}

/* Rate controller aborts if it detects server or network failure. */
TEST(failure, many_errors_cause_abort) {
  RateController dut;
  EXPECT_FALSE(dut.ServerHasFailed());
  RateController::clock::time_point t = RateController::clock::now();
  RateController::clock::duration interval = std::chrono::seconds(2);
  for (int i = 0; i < 30; i++) {
    dut.RequestCompleted(t, t + interval, false);
    t += interval;
  }
  EXPECT_TRUE(dut.ServerHasFailed());
}

/* Rate controller continues through intermittent errors. */
TEST(failure, continues_through_occasional_errors) {
  RateController dut;
  EXPECT_FALSE(dut.ServerHasFailed());
  RateController::clock::time_point t = RateController::clock::now();
  RateController::clock::duration interval = std::chrono::seconds(2);
  for (int i = 0; i < 1000; i++) {
    bool ok = i % 30 != 0;
    dut.RequestCompleted(t, t + interval, ok);
    t += interval;
    EXPECT_FALSE(dut.ServerHasFailed());
  }
}

/* Rate controller improves concurrency when network conditions are good. */
TEST(control, good_results_improve_concurrency) {
  RateController dut;
  RateController::clock::time_point t = RateController::clock::now();
  RateController::clock::duration interval = std::chrono::seconds(2);
  int initial_concurrency = dut.MaxConcurrency();
  for (int i = 0; i < 10; i++) {
    dut.RequestCompleted(t, t + interval, true);
    t += interval;
  }
  EXPECT_GT(dut.MaxConcurrency(), initial_concurrency);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
