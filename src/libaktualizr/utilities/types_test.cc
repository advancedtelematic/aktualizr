#include <gtest/gtest.h>

#include "utilities/types.h"

TimeStamp now("2017-01-01T01:00:00Z");

/* Parse Uptane timestamps. */
TEST(TimeStamp, Parsing) {
  TimeStamp t_old("2038-01-19T02:00:00Z");
  TimeStamp t_new("2038-01-19T03:14:06Z");

  TimeStamp t_invalid;

  EXPECT_LT(t_old, t_new);
  EXPECT_GT(t_new, t_old);
  EXPECT_FALSE(t_invalid < t_old);
  EXPECT_FALSE(t_old < t_invalid);
  EXPECT_FALSE(t_invalid < t_invalid);
}

/* Throw an exception if an Uptane timestamp is invalid. */
TEST(TimeStamp, ParsingInvalid) { EXPECT_THROW(TimeStamp("2038-01-19T0"), TimeStamp::InvalidTimeStamp); }

/* Get current time. */
TEST(TimeStamp, Now) {
  TimeStamp t_past("1982-12-13T02:00:00Z");
  TimeStamp t_future("2038-01-19T03:14:06Z");
  TimeStamp t_now(TimeStamp::Now());

  EXPECT_LT(t_past, t_now);
  EXPECT_LT(t_now, t_future);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
