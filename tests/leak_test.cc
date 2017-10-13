#include <gtest/gtest.h>

/**
 * A test case that leaks memory, to check that we can spot this in valgrind
 */
TEST(Leak, ThisTestLeaks) { EXPECT_TRUE(new int[45]); }

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif