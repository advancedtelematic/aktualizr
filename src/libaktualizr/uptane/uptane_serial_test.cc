#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <libaktualizr/utils.h>

#include "httpfake.h"
#include "logging/logging.h"
#include "primary/reportqueue.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "test_utils.h"
#include "uptane/tuf.h"
#include "uptane/uptanerepository.h"
#include "uptane_test_common.h"

namespace bpo = boost::program_options;

/**
 * Check that aktualizr generates random ecu_serial for Primary and all
 * Secondaries.
 */
TEST(Uptane, RandomSerial) {
  RecordProperty("zephyr_key", "OTA-989,TST-155");
  // Make two configs, neither of which specify a Primary serial.
  TemporaryDirectory temp_dir1, temp_dir2;
  Config conf_1("tests/config/basic.toml");
  conf_1.storage.path = temp_dir1.Path();
  Config conf_2("tests/config/basic.toml");
  conf_2.storage.path = temp_dir2.Path();

  conf_1.provision.primary_ecu_serial = "";
  conf_2.provision.primary_ecu_serial = "";

  UptaneTestCommon::addDefaultSecondary(conf_1, temp_dir1, "", "secondary_hardware", false);
  UptaneTestCommon::addDefaultSecondary(conf_2, temp_dir2, "", "secondary_hardware", false);

  // Initialize.
  auto storage_1 = INvStorage::newStorage(conf_1.storage);
  auto storage_2 = INvStorage::newStorage(conf_2.storage);
  auto http1 = std::make_shared<HttpFake>(temp_dir1.Path());
  auto http2 = std::make_shared<HttpFake>(temp_dir2.Path());

  auto uptane_client1 = std_::make_unique<UptaneTestCommon::TestUptaneClient>(conf_1, storage_1, http1);
  ASSERT_NO_THROW(uptane_client1->initialize());

  auto uptane_client2 = std_::make_unique<UptaneTestCommon::TestUptaneClient>(conf_2, storage_2, http2);
  ASSERT_NO_THROW(uptane_client2->initialize());

  // Verify that none of the serials match.
  EcuSerials ecu_serials_1;
  EcuSerials ecu_serials_2;
  ASSERT_TRUE(storage_1->loadEcuSerials(&ecu_serials_1));
  ASSERT_TRUE(storage_2->loadEcuSerials(&ecu_serials_2));
  ASSERT_EQ(ecu_serials_1.size(), 2);
  ASSERT_EQ(ecu_serials_2.size(), 2);
  EXPECT_FALSE(ecu_serials_1[0].first.ToString().empty());
  EXPECT_FALSE(ecu_serials_1[1].first.ToString().empty());
  EXPECT_FALSE(ecu_serials_2[0].first.ToString().empty());
  EXPECT_FALSE(ecu_serials_2[1].first.ToString().empty());
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_NE(ecu_serials_1[1].first, ecu_serials_2[1].first);
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[1].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[1].first);
}

/**
 * Check that aktualizr saves random ecu_serial for Primary and all Secondaries.
 *
 * Test with a virtual Secondary.
 */
TEST(Uptane, ReloadSerial) {
  RecordProperty("zephyr_key", "OTA-989,TST-156");
  TemporaryDirectory temp_dir;
  EcuSerials ecu_serials_1;
  EcuSerials ecu_serials_2;

  // Initialize. Should store new serials.
  {
    Config conf("tests/config/basic.toml");
    conf.storage.path = temp_dir.Path();
    conf.provision.primary_ecu_serial = "";

    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());

    UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "", "secondary_hardware", false);
    auto uptane_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(conf, storage, http);

    ASSERT_NO_THROW(uptane_client->initialize());
    ASSERT_TRUE(storage->loadEcuSerials(&ecu_serials_1));
    ASSERT_EQ(ecu_serials_1.size(), 2);
    EXPECT_FALSE(ecu_serials_1[0].first.ToString().empty());
    EXPECT_FALSE(ecu_serials_1[1].first.ToString().empty());
  }

  // Keep storage directory, but initialize new objects. Should load existing
  // serials.
  {
    Config conf("tests/config/basic.toml");
    conf.storage.path = temp_dir.Path();
    conf.provision.primary_ecu_serial = "";

    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());
    UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "", "secondary_hardware", false);
    auto uptane_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(conf, storage, http);

    ASSERT_NO_THROW(uptane_client->initialize());
    ASSERT_TRUE(storage->loadEcuSerials(&ecu_serials_2));
    ASSERT_EQ(ecu_serials_2.size(), 2);
    EXPECT_FALSE(ecu_serials_2[0].first.ToString().empty());
    EXPECT_FALSE(ecu_serials_2[1].first.ToString().empty());
  }

  // Verify that serials match across initializations.
  EXPECT_EQ(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_EQ(ecu_serials_1[1].first, ecu_serials_2[1].first);
  // Sanity check that Primary and Secondary serials do not match.
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[1].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[1].first);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif
