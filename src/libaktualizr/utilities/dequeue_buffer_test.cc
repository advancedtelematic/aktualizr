#include <gtest/gtest.h>

#include <string>
#include "utilities/dequeue_buffer.h"

TEST(DequeueBuffer, Simple) {
  DequeueBuffer dut;

  EXPECT_NE(dut.Head(), nullptr);
  EXPECT_EQ(dut.Size(), 0);

  size_t chars = static_cast<size_t>(snprintf(dut.Tail(), dut.TailSpace(), "hello "));
  dut.HaveEnqueued(chars);
  chars = static_cast<size_t>(snprintf(dut.Tail(), dut.TailSpace(), "world"));
  dut.HaveEnqueued(chars);
  EXPECT_EQ(dut.Size(), strlen("hello world"));

  EXPECT_EQ(std::string(dut.Head(), dut.Size()), "hello world");
  dut.Consume(3);
  EXPECT_EQ(std::string(dut.Head(), dut.Size()), "lo world");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
