#include "rate_controller.h"
#include <gtest/gtest.h>

TEST(initial, initially_ok) {
  RateController dut;
  EXPECT_FALSE(dut.ServerHasFailed());
}

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