#include <gtest/gtest.h>

#include <boost/shared_ptr.hpp>

#include "uptane/secondaryconfig.h"
#include "uptane/secondaryfactory.h"
#include "uptane/secondaryinterface.h"

TEST(SecondaryFactory, Virtual) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::kVirtual;
  sconfig.partial_verifying = false;
  sconfig.full_client_dir = temp_dir.Path();
  sconfig.ecu_serial = "";
  sconfig.ecu_hardware_id = "secondary_hardware";
  sconfig.ecu_private_key = "sec.priv";
  sconfig.ecu_public_key = "sec.pub";
  sconfig.firmware_path = temp_dir.Path() / "firmware.txt";
  sconfig.target_name_path = temp_dir.Path() / "firmware_name.txt";
  sconfig.metadata_path = temp_dir.Path() / "metadata";
  boost::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(sconfig);
  EXPECT_TRUE(sec);
}

TEST(SecondaryFactory, Legacy) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::kLegacy;
  sconfig.partial_verifying = false;
  sconfig.full_client_dir = temp_dir.Path();
  sconfig.ecu_serial = "";
  sconfig.ecu_hardware_id = "secondary_hardware";
  sconfig.ecu_private_key = "sec.priv";
  sconfig.ecu_public_key = "sec.pub";
  sconfig.firmware_path = temp_dir.Path() / "firmware.txt";
  sconfig.target_name_path = temp_dir.Path() / "firmware_name.txt";
  sconfig.metadata_path = temp_dir.Path() / "metadata";
  boost::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(sconfig);
  EXPECT_TRUE(sec);
}

TEST(SecondaryFactory, Bad) {
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = (Uptane::SecondaryType)-1;
  boost::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(sconfig);
  EXPECT_FALSE(sec);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
