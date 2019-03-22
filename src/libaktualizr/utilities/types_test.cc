#include <gtest/gtest.h>

#include "utilities/types.h"

TimeStamp now("2017-01-01T01:00:00Z");

/* Parse Uptane timestamps. */
TEST(Types, TimeStampParsing) {
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
TEST(Types, TimeStampParsingInvalid) { EXPECT_THROW(TimeStamp("2038-01-19T0"), TimeStamp::InvalidTimeStamp); }

/* Get current time. */
TEST(Types, TimeStampNow) {
  TimeStamp t_past("1982-12-13T02:00:00Z");
  TimeStamp t_future("2038-01-19T03:14:06Z");
  TimeStamp t_now(TimeStamp::Now());

  EXPECT_LT(t_past, t_now);
  EXPECT_LT(t_now, t_future);
}

TEST(Types, ResultCode) {
  data::ResultCode ok_res{data::ResultCode::Numeric::kOk};
  EXPECT_EQ(ok_res.num_code, data::ResultCode::Numeric::kOk);
  EXPECT_EQ(ok_res.toString(), "OK");
  std::string repr = ok_res.toRepr();
  EXPECT_EQ(repr, "\"OK\":0");
  EXPECT_EQ(data::ResultCode::fromRepr(repr), ok_res);

  // legacy format
  EXPECT_EQ(data::ResultCode::fromRepr("OK:0"), ok_res);

  // !
  EXPECT_NE(ok_res, data::ResultCode(data::ResultCode::Numeric::kOk, "OK2"));
  EXPECT_NE(ok_res, data::ResultCode(data::ResultCode::Numeric::kGeneralError, "OK"));
  EXPECT_EQ(data::ResultCode::fromRepr("OK"), data::ResultCode(data::ResultCode::Numeric::kUnknown, "OK"));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
