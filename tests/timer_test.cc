#include <gtest/gtest.h>

#include <sstream>

#include "utilities/timer.h"

TEST(Timer, Short) {
  Timer timer;
  EXPECT_FALSE(timer.RunningMoreThan(1.0));
  sleep(2.0);
  EXPECT_TRUE(timer.RunningMoreThan(1.0));

  std::stringstream ss;
  ss << timer;
  std::string res = ss.str();
  EXPECT_EQ(res[0], '2');         // sleep(2) should take between 2 and 3 seconds
  EXPECT_EQ(*res.rbegin(), 's');  // last character is 's'
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
