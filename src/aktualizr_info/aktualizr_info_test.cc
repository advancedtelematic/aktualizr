#include <gtest/gtest.h>

#include <boost/process.hpp>

#include "config/config.h"
#include "storage/sqlstorage.h"
#include "utilities/utils.h"

class AktualizrInfoTest : public ::testing::Test {
 protected:
  AktualizrInfoTest() : test_conf_file_{test_dir_ / "conf.toml"}, test_db_file_{test_dir_ / "sql.db"} {
    config_.pacman.type = PackageManager::kNone;
    config_.storage.path = test_dir_.PathString();
    config_.storage.sqldb_path = test_db_file_;
    config_.logger.loglevel = boost::log::trivial::error;
    // Config ctor sets the log threshold to a default value (info) so we need to reset it to the desired one
    logger_set_threshold(config_.logger);

    // dump a config into a toml file so the executable can use it as an input configuration
    boost::filesystem::ofstream conf_file(test_conf_file_);
    config_.writeToStream(conf_file);
    conf_file.close();

    // create a storage and a storage file
    db_storage_ = INvStorage::newStorage(config_.storage);
  }

  virtual void SetUp() {
    device_id = "aktualizr-info-test-device_ID-fd1fc55c-3abc-4de8-a2ca-32d455ae9c11";
    primary_ecu_serial = "82697cac-f54c-40ea-a8f2-76c203b7bf2f";
    primary_ecu_id = "primary-hdwr-e96c08e0-38a0-4903-a021-143cf5427bc9";

    db_storage_->storeDeviceId(device_id);
  }

  virtual void TearDown() {}

  void SpawnProcess(const char* arg = nullptr) {
    std::future<std::string> output;
    std::future<std::string> err_output;
    boost::asio::io_service io_service;
    int child_process_exit_code = -1;

    if (arg != nullptr) {
      executable_args.push_back(arg);
    }
    aktualizr_info_output.clear();

    try {
      boost::process::child aktualizr_info_child_process(
          boost::process::exe = executable_to_run, boost::process::args = executable_args,
          boost::process::std_out > output, boost::process::std_err > err_output, io_service);

      io_service.run();

      // To get the child process exit code we need to wait even in the async case (specifics of boost::process::child)
      ASSERT_TRUE(aktualizr_info_child_process.wait_for(std::chrono::seconds(20)));
      child_process_exit_code = aktualizr_info_child_process.exit_code();
    } catch (const std::exception& exc) {
      FAIL() << "Failed to spawn process " << executable_to_run << " exited with an error: " << exc.what();
    }

    ASSERT_EQ(child_process_exit_code, 0)
        << "Process " << executable_to_run << " exited with an error: " << err_output.get();

    aktualizr_info_output = output.get();
    LOG_TRACE << "\n" << aktualizr_info_output << "\n";
  }

 protected:
  TemporaryDirectory test_dir_;
  boost::filesystem::path test_conf_file_;
  boost::filesystem::path test_db_file_;

  Config config_;
  std::shared_ptr<INvStorage> db_storage_;

  const std::string executable_to_run = "./aktualizr-info";
  std::vector<std::string> executable_args = {"-c", test_conf_file_.string()};
  std::string aktualizr_info_output;

  std::string device_id;
  std::string primary_ecu_serial;
  std::string primary_ecu_id;
};

/**
 * Print device ID
 * Print primary ECU serial
 * Print primary ECU hardware ID
 * Print secondary ECU serials
 * Print secondary ECU hardware IDs
 * Print provisioning status if provisioned
 * Print whether metadata has been fetched from the server if they actually were fetched
 */
TEST_F(AktualizrInfoTest, PrintPrimaryAndSecondaryInfo) {
  const std::string secondary_ecu_serial = "c6998d3e-2a68-4ac2-817e-4ea6ef87d21f";
  const std::string secondary_ecu_id = "secondary-hdwr-af250269-bd6f-4148-9426-4101df7f613a";
  const std::string provisioning_status = "Provisioned on server: yes";
  const std::string fetched_metadata = "Fetched metadata: yes";

  Json::Value meta_root;
  std::string director_root = Utils::jsonToStr(meta_root);

  db_storage_->storeEcuSerials(
      {{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)},
       {Uptane::EcuSerial(secondary_ecu_serial), Uptane::HardwareIdentifier(secondary_ecu_id)}});
  db_storage_->storeEcuRegistered();
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  SpawnProcess();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find(secondary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(secondary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find(provisioning_status), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(fetched_metadata), std::string::npos);
}

/**
 * Print provisioning status if not provisioned
 * Print whether metadata has been fetched from the server if they actually weren't fetched
 */
TEST_F(AktualizrInfoTest, PrintProvisioningAndMetadataNegative) {
  const std::string provisioning_status = "Provisioned on server: no";
  const std::string fetched_metadata = "Fetched metadata: no";

  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});

  SpawnProcess();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find(provisioning_status), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(fetched_metadata), std::string::npos);
}

/**
 * Print secondary ECUs no longer accessible (old)
 * Print secondary ECUs registered after provisioning (not registered)
 */
TEST_F(AktualizrInfoTest, PrintSecondaryNotRegisteredOrRemoved) {
  const std::string provisioning_status = "Provisioned on server: yes";

  const std::string secondary_ecu_serial = "c6998d3e-2a68-4ac2-817e-4ea6ef87d21f";
  const std::string secondary_ecu_id = "secondary-hdwr-af250269-bd6f-4148-9426-4101df7f613a";

  const std::string secondary_ecu_serial_not_reg = "18b018a1-fdda-4461-a281-42237256cc2f";
  const std::string secondary_ecu_id_not_reg = "secondary-hdwr-cbce3a7a-7cbb-4da4-9fff-8e10e5c3de98";

  const std::string secondary_ecu_serial_old = "c2191c12-7298-4be3-b781-d223dac7f75e";
  const std::string secondary_ecu_id_old = "secondary-hdwr-0ded1c51-d280-49c3-a92b-7ff2c2e91d8c";

  db_storage_->storeEcuSerials(
      {{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)},
       {Uptane::EcuSerial(secondary_ecu_serial), Uptane::HardwareIdentifier(secondary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  db_storage_->storeMisconfiguredEcus({{Uptane::EcuSerial(secondary_ecu_serial_not_reg),
                                        Uptane::HardwareIdentifier(secondary_ecu_id_not_reg), EcuState::kNotRegistered},
                                       {Uptane::EcuSerial(secondary_ecu_serial_old),
                                        Uptane::HardwareIdentifier(secondary_ecu_id_old), EcuState::kOld}});

  SpawnProcess();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("\'18b018a1-fdda-4461-a281-42237256cc2f\' with hardware_id "
                                       "\'secondary-hdwr-cbce3a7a-7cbb-4da4-9fff-8e10e5c3de98\'"
                                       " not registered yet"),
            std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("\'c2191c12-7298-4be3-b781-d223dac7f75e\' with hardware_id "
                                       "\'secondary-hdwr-0ded1c51-d280-49c3-a92b-7ff2c2e91d8c\'"
                                       " has been removed from config"),
            std::string::npos);
}

/**
 * Print root metadata from images repository
 */
TEST_F(AktualizrInfoTest, PrintImageRootMetadata) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  Json::Value images_root_json;
  images_root_json["asdas"] = "asdad";

  std::string images_root = Utils::jsonToStr(images_root_json);
  db_storage_->storeRoot(images_root, Uptane::RepositoryType::Image(), Uptane::Version(1));
  db_storage_->storeRoot(images_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  SpawnProcess("--images-root");
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("image root.json content:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(images_root), std::string::npos);
}

/**
 * Print root metadata from director repository
 */
TEST_F(AktualizrInfoTest, PrintDirectorRootMetadata) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  Json::Value images_root_json;
  images_root_json["asdas"] = "asdad";

  std::string images_root = Utils::jsonToStr(images_root_json);
  db_storage_->storeRoot(images_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  SpawnProcess("--director-root");
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("director root.json content:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(images_root), std::string::npos);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
