#include <gtest/gtest.h>

#include "metafake.h"
#include "partialverificationsecondary.h"
#include "primary/secondaryinterface.h"
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

class PartialVerificationSecondaryTest : public ::testing::Test {
 protected:
  PartialVerificationSecondaryTest() {
    config_.partial_verifying = true;
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
  Primary::PartialVerificationSecondaryConfig config_;
};

/* Create a virtual secondary for testing. */
TEST_F(VirtualSecondaryTest, Instantiation) { EXPECT_NO_THROW(Primary::VirtualSecondary virtual_sec(config_)); }

/* Partial verification secondaries generate and store public keys. */
TEST_F(PartialVerificationSecondaryTest, Uptane_get_key) {
  Uptane::PartialVerificationSecondary sec1(config_);
  PublicKey key1 = sec1.getPublicKey();
  Uptane::PartialVerificationSecondary sec2(config_);
  PublicKey key2 = sec2.getPublicKey();
  // Verify that we store keys
  EXPECT_EQ(key1, key2);
}

// TODO(OTA-2484): restore these tests when the implementation is actually functional.
#if 0
/* Partial verification secondaries can verify Uptane metadata. */
TEST_F(PartialVerificationSecondaryTest, Uptane_putMetadata_good) {
  Uptane::PartialVerificationSecondary sec(config_);
  Uptane::RawMetaPack metadata;

  TemporaryDirectory temp_dir;
  MetaFake meta(temp_dir.Path());
  metadata.director_root = Utils::readFile(temp_dir / "director/root.json");
  metadata.director_targets = Utils::readFile(temp_dir / "director/targets_hasupdates.json");
  EXPECT_NO_THROW(sec.putMetadata(metadata));
}

/* Partial verification secondaries reject invalid Uptane metadata. */
TEST_F(PartialVerificationSecondaryTest, Uptane_putMetadata_bad) {
  Uptane::PartialVerificationSecondary sec(config_);
  Uptane::RawMetaPack metadata;

  TemporaryDirectory temp_dir;
  MetaFake meta(temp_dir.Path());
  metadata.director_root = Utils::readFile(temp_dir / "director/root.json");

  Json::Value json_targets = Utils::parseJSONFile(temp_dir / "director/targets_hasupdates.json");
  json_targets["signatures"][0]["sig"] = "Wrong signature";
  metadata.director_targets = Utils::jsonToStr(json_targets);
  EXPECT_THROW(sec.putMetadata(metadata), Uptane::BadKeyId);
}
#endif

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
