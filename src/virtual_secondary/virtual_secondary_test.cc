#include <gtest/gtest.h>

#include "libaktualizr/secondaryinterface.h"

#include "utilities/utils.h"
#include "virtualsecondary.h"

class VirtualSecondaryTest : public ::testing::Test {
 protected:
  VirtualSecondaryTest() {
    config_.partial_verifying = false;
    config_.full_client_dir = temp_dir_.Path();
    config_.ecu_serial = "";
    config_.ecu_hardware_id = "secondary_hardware";
    config_.ecu_private_key = "sec.priv";
    config_.ecu_public_key = "sec.pub";
    config_.firmware_path = temp_dir_.Path() / "firmware.txt";
    config_.target_name_path = temp_dir_.Path() / "firmware_name.txt";
    config_.metadata_path = temp_dir_.Path() / "metadata";
  }

  virtual void SetUp() {}
  virtual void TearDown() {}

 protected:
  TemporaryDirectory temp_dir_;
  Primary::VirtualSecondaryConfig config_;
};

/* Create a virtual secondary for testing. */
TEST_F(VirtualSecondaryTest, Instantiation) { EXPECT_NO_THROW(Primary::VirtualSecondary virtual_sec(config_)); }

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
