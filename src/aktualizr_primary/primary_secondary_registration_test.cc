#include <gtest/gtest.h>

#include "httpfake.h"
#include "primary/aktualizr.h"
#include "secondary.h"
#include "uptane_test_common.h"
#include "utilities/utils.h"

boost::filesystem::path uptane_repos_dir;
boost::filesystem::path fake_meta_dir;

/* This tests that a device that had an IP Secondary will still find it after
 * recent changes, even if it does not connect when the device starts. */
TEST(PrimarySecondaryReg, SecondariesMigration) {
  const Uptane::EcuSerial primary_serial{"p_serial"};
  const Uptane::EcuSerial secondary_serial{"s_serial"};
  const Uptane::HardwareIdentifier primary_hwid{"p_hwid"};
  const Uptane::HardwareIdentifier secondary_hwid{"s_hwid"};

  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates", fake_meta_dir);
  const auto& url = http->tls_server;

  Config conf("tests/config/basic.toml");
  conf.uptane.director_server = url + "/director";
  conf.uptane.repo_server = url + "/repo";
  conf.provision.server = url;
  conf.provision.primary_ecu_serial = primary_serial.ToString();
  conf.provision.primary_ecu_hardware_id = primary_hwid.ToString();
  conf.storage.path = temp_dir.Path();
  conf.import.base_path = temp_dir.Path() / "import";
  conf.tls.server = url;
  conf.bootloader.reboot_sentinel_dir = temp_dir.Path();
  const boost::filesystem::path sec_conf_path = temp_dir / "s_config.json";
  conf.uptane.secondary_config_file = sec_conf_path;

  auto storage = INvStorage::newStorage(conf.storage);
  Json::Value sec_conf;

  {
    // prepare storage the "old" way:
    storage->storeDeviceId("device");
    storage->storeEcuSerials({{primary_serial, primary_hwid}, {secondary_serial, secondary_hwid}});
    storage->storeEcuRegistered();

    sec_conf["IP"]["secondary_wait_port"] = 9030;
    sec_conf["IP"]["secondary_wait_timeout"] = 1;
    sec_conf["IP"]["secondaries"] = Json::arrayValue;
    sec_conf["IP"]["secondaries"][0]["addr"] = "127.0.0.1:9061";
    Utils::writeFile(sec_conf_path, sec_conf);
  }

  {
    // verifies that aktualizr can still start if it can't connect to its
    // secondary

    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    Primary::initSecondaries(aktualizr, sec_conf_path);
    aktualizr.Initialize();
    aktualizr.CheckUpdates().get();

    std::vector<SecondaryInfo> secs_info;
    storage->loadSecondariesInfo(&secs_info);
    EXPECT_EQ(secs_info[0].serial.ToString(), secondary_serial.ToString());
    EXPECT_EQ(secs_info[0].type, "IP");
    EXPECT_EQ(secs_info[0].extra, R"({"ip":"127.0.0.1","port":9061})");
  }

  const Uptane::EcuSerial secondary_serial2{"s_serial2"};
  const Uptane::HardwareIdentifier secondary_hwid2{"s_hwid2"};
  {
    // prepare the storage the "old" way with two secondaries
    storage->clearEcuSerials();
    storage->storeEcuSerials({
        {primary_serial, primary_hwid},
        {secondary_serial, secondary_hwid},
        {secondary_serial2, secondary_hwid2},
    });

    sec_conf["IP"]["secondaries"][1]["addr"] = "127.0.0.1:9062";
    Utils::writeFile(sec_conf_path, sec_conf);
  }

  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

    // this will fail to actually register the secondaries as there is no way to
    // tell them apart (since they haven't connected)
    // however, we still allow aktualizr to go through in case we would like to
    // update the primary, to maybe fix this situation
    Primary::initSecondaries(aktualizr, sec_conf_path);
    aktualizr.Initialize();
    aktualizr.CheckUpdates().get();

    // there was no way to correlate info here
    std::vector<SecondaryInfo> secs_info;
    storage->loadSecondariesInfo(&secs_info);
    EXPECT_EQ(secs_info[0].serial.ToString(), secondary_serial.ToString());
    EXPECT_EQ(secs_info[0].type, "");
    EXPECT_EQ(secs_info[0].extra, "");
    EXPECT_EQ(secs_info[1].serial.ToString(), secondary_serial2.ToString());
    EXPECT_EQ(secs_info[1].type, "");
    EXPECT_EQ(secs_info[1].extra, "");
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the base directory of Uptane repos.\n";
    return EXIT_FAILURE;
  }
  uptane_repos_dir = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  TemporaryDirectory tmp_dir;
  fake_meta_dir = tmp_dir.Path();
  MetaFake meta_fake(fake_meta_dir);

  return RUN_ALL_TESTS();
}
#endif
