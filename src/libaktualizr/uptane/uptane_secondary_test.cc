#include <gtest/gtest.h>

#include "uptane/partialverificationsecondary.h"
#include "uptane/secondaryconfig.h"
#include "uptane/secondaryfactory.h"
#include "uptane/secondaryinterface.h"

TEST(SecondaryFactory, Virtual) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::SecondaryType::kVirtual;
  sconfig.partial_verifying = false;
  sconfig.full_client_dir = temp_dir.Path();
  sconfig.ecu_serial = "";
  sconfig.ecu_hardware_id = "secondary_hardware";
  sconfig.ecu_private_key = "sec.priv";
  sconfig.ecu_public_key = "sec.pub";
  sconfig.firmware_path = temp_dir.Path() / "firmware.txt";
  sconfig.target_name_path = temp_dir.Path() / "firmware_name.txt";
  sconfig.metadata_path = temp_dir.Path() / "metadata";
  std::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(sconfig);
  EXPECT_TRUE(sec);
}

TEST(SecondaryFactory, Legacy) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::SecondaryType::kLegacy;
  sconfig.partial_verifying = false;
  sconfig.full_client_dir = temp_dir.Path();
  sconfig.ecu_serial = "";
  sconfig.ecu_hardware_id = "secondary_hardware";
  sconfig.ecu_private_key = "sec.priv";
  sconfig.ecu_public_key = "sec.pub";
  sconfig.firmware_path = temp_dir.Path() / "firmware.txt";
  sconfig.target_name_path = temp_dir.Path() / "firmware_name.txt";
  sconfig.metadata_path = temp_dir.Path() / "metadata";
  std::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(sconfig);
  EXPECT_TRUE(sec);
}

TEST(SecondaryFactory, Uptane_get_key) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::SecondaryType::kVirtualUptane;
  sconfig.partial_verifying = true;
  sconfig.full_client_dir = temp_dir.Path();
  sconfig.ecu_serial = "";
  sconfig.ecu_hardware_id = "secondary_hardware";
  sconfig.ecu_private_key = "sec.priv";
  sconfig.ecu_public_key = "sec.pub";
  sconfig.firmware_path = temp_dir.Path() / "firmware.txt";
  sconfig.target_name_path = temp_dir.Path() / "firmware_name.txt";
  sconfig.metadata_path = temp_dir.Path() / "metadata";
  Uptane::PartialVerificationSecondary sec1(sconfig);
  PublicKey key1 = sec1.getPublicKey();
  Uptane::PartialVerificationSecondary sec2(sconfig);
  PublicKey key2 = sec2.getPublicKey();
  // Verify that we store keys
  EXPECT_EQ(key1, key2);
}

TEST(SecondaryFactory, Uptane_putMetadata_good) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::SecondaryType::kVirtualUptane;
  sconfig.partial_verifying = true;
  sconfig.full_client_dir = temp_dir.Path();
  sconfig.ecu_serial = "";
  sconfig.ecu_hardware_id = "secondary_hardware";
  sconfig.ecu_private_key = "sec.priv";
  sconfig.ecu_public_key = "sec.pub";
  sconfig.firmware_path = temp_dir.Path() / "firmware.txt";
  sconfig.target_name_path = temp_dir.Path() / "firmware_name.txt";
  sconfig.metadata_path = temp_dir.Path() / "metadata";
  Uptane::PartialVerificationSecondary sec(sconfig);
  Uptane::RawMetaPack metadata;

  metadata.director_root = Utils::readFile("tests/test_data/repo/repo/director/root.json");
  metadata.director_targets = Utils::readFile("tests/test_data/repo/repo/director/targets_hasupdates.json");
  EXPECT_NO_THROW(sec.putMetadata(metadata));
}

TEST(SecondaryFactory, Uptane_putMetadata_bad) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::SecondaryType::kVirtualUptane;
  sconfig.partial_verifying = true;
  sconfig.full_client_dir = temp_dir.Path();
  sconfig.ecu_serial = "";
  sconfig.ecu_hardware_id = "secondary_hardware";
  sconfig.ecu_private_key = "sec.priv";
  sconfig.ecu_public_key = "sec.pub";
  sconfig.firmware_path = temp_dir.Path() / "firmware.txt";
  sconfig.target_name_path = temp_dir.Path() / "firmware_name.txt";
  sconfig.metadata_path = temp_dir.Path() / "metadata";
  Uptane::PartialVerificationSecondary sec(sconfig);
  Uptane::RawMetaPack metadata;

  metadata.director_root = Utils::readFile("tests/test_data/repo/repo/director/root.json");

  Json::Value json_targets = Utils::parseJSONFile("tests/test_data/repo/repo/director/targets_hasupdates.json");
  json_targets["signatures"][0]["sig"] = "Wrong signature";
  metadata.director_targets = Utils::jsonToStr(json_targets);
  EXPECT_THROW(sec.putMetadata(metadata), Uptane::UnmetThreshold);
}

TEST(SecondaryFactory, Bad) {
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = (Uptane::SecondaryType)-1;
  std::shared_ptr<Uptane::SecondaryInterface> sec = Uptane::SecondaryFactory::makeSecondary(sconfig);
  EXPECT_FALSE(sec);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
