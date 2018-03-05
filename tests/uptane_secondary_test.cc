#include <gtest/gtest.h>

#include <boost/shared_ptr.hpp>

#include "uptane/partialverificationsecondary.h"
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

TEST(SecondaryFactory, Uptane_get_key) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::kUptane;
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
  std::string key1 = sec1.getPublicKey();
  Uptane::PartialVerificationSecondary sec2(sconfig);
  std::string key2 = sec2.getPublicKey();
  EXPECT_FALSE(key1.empty());
  EXPECT_FALSE(key2.empty());
  // Verify that we store keys
  EXPECT_EQ(key1, key2);
}

TEST(SecondaryFactory, Uptane_putMetadata_good) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::kUptane;
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
  Uptane::MetaPack metadata;

  Json::Value json_root = Utils::parseJSONFile("tests/test_data/repo/root.json");
  metadata.director_root = Uptane::Root("director", json_root);

  Json::Value json_targets = Utils::parseJSONFile("tests/test_data/targets_hasupdates.json");
  metadata.director_targets = Uptane::Targets(json_targets);
  EXPECT_NO_THROW(sec.putMetadata(metadata));
}

TEST(SecondaryFactory, Uptane_putMetadata_bad) {
  TemporaryDirectory temp_dir;
  Uptane::SecondaryConfig sconfig;
  sconfig.secondary_type = Uptane::kUptane;
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
  Uptane::MetaPack metadata;

  Json::Value json_root = Utils::parseJSONFile("tests/test_data/repo/root.json");
  metadata.director_root = Uptane::Root("director", json_root);

  Json::Value json_targets = Utils::parseJSONFile("tests/test_data/targets_hasupdates.json");
  json_targets["signatures"][0]["sig"] = "Wrong signature";
  metadata.director_targets = Uptane::Targets(json_targets);
  EXPECT_THROW(sec.putMetadata(metadata), Uptane::UnmetThreshold);
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
