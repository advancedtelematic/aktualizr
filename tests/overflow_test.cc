#include <gtest/gtest.h>

void overflow(int* array, const int index) { array[index]++; }

/**
 * A test case that overflows an array, to check that we catch this with
 * -fsanitize=undefined -fsanitize=bounds.
 */
TEST(Overflow, ThisTestOverflows) {
  int array[10];
  overflow(array, 10);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
