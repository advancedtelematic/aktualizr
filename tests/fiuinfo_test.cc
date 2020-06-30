#include <gtest/gtest.h>
#include <unistd.h>

#include "utilities/fault_injection.h"
#include "utilities/utils.h"

std::string fiu_script;

TEST(Fiuinfo, PassInfoWithRun) {
  EXPECT_TRUE(fiu_fail("fail"));
  EXPECT_EQ(fault_injection_last_info(), "fiurun_failure");

  // check that `fiu ctrl` overrides value at `run`
  Utils::shell(fiu_script + " ctrl -c 'enable name=fail,failinfo=failure2' " + std::to_string(getpid()), nullptr);
  EXPECT_TRUE(fiu_fail("fail"));
  EXPECT_EQ(fault_injection_last_info(), "failure2");

  // check that it can be controlled with fault_injection_enable
  fault_injection_enable("fail", 1, "failure3", 0);
  EXPECT_TRUE(fiu_fail("fail"));
  EXPECT_EQ(fault_injection_last_info(), "failure3");

  fiu_disable("fail");
  EXPECT_FALSE(fiu_fail("fail"));
}

TEST(Fiuinfo, PassInfoWithCtrl) {
  EXPECT_FALSE(fiu_fail("failctrl"));
  Utils::shell(fiu_script + " ctrl -c 'enable name=failctrl,failinfo=test_ctrl' " + std::to_string(getpid()), nullptr);
  EXPECT_TRUE(fiu_fail("failctrl"));

  Utils::shell(fiu_script + " ctrl -c 'disable name=failctrl' " + std::to_string(getpid()), nullptr);
  EXPECT_FALSE(fiu_fail("failctrl"));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the fiu wrapper script.\n";
    return EXIT_FAILURE;
  }
  fiu_script = argv[1];

  return RUN_ALL_TESTS();
}
#endif
