#include <gtest/gtest.h>

#include <vector>

/**
 * A test case that overflows a vector, to check that we catch this with
 * -fstack-protector-all.
 */
TEST(Overflow, ThisTestOverflows) {
  int count = 10;
  std::vector<int> array(count, 0);
  for (int i = 0; i <= count; ++i) {
    array[i]++;
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
