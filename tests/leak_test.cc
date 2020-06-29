#include <gtest/gtest.h>

/**
 * A test case that leaks memory, to check that we can spot this in valgrind
 */
TEST(Leak, ThisTestLeaks) {
  int* temp = new int[45];
  int temp2 = *temp++;
  std::cout << temp2;
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
