#include <gtest/gtest.h>

#include <boost/asio/io_service.hpp>
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
    // set it into 'trace' to see the aktualizr-info output
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

  void SpawnProcess(const std::vector<std::string> args = std::vector<std::string>{}) {
    std::future<std::string> output;
    std::future<std::string> err_output;
    boost::asio::io_service io_service;
    int child_process_exit_code = -1;

    executable_args.insert(std::end(executable_args), std::begin(args), std::end(args));
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
 * Verifies an output of the aktualizr-info in a positive case when
 * there are both primary and secondary present and a device is provisioned
 * and metadata are fetched from a server
 *
 * Checks actions:
 *
 *  - [x] Print device ID
 *  - [x] Print primary ECU serial
 *  - [x] Print primary ECU hardware ID
 *  - [x] Print secondary ECU serials
 *  - [x] Print secondary ECU hardware IDs
 *  - [x] Print provisioning status, if provisioned
 *  - [x] Print whether metadata has been fetched from the server, if they were fetched
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
 * Verifies an output of aktualizr-info if a device is not provisioned and metadata are not fetched
 *
 * Checks actions:
 *
 *  - [x] Print provisioning status, if not provisioned
 *  - [x] Print whether metadata has been fetched from the server, if they were not fetched
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
 * Verifies an output of miscofigured secondary ECUs
 *
 * Checks actions:
 *
 * - [x] Print secondary ECUs no longer accessible (miscofigured: old)
 * - [x] Print secondary ECUs registered after provisioning (not registered)
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
 * Verifies aktualizr-info output of a root metadata from the images repository
 *
 * Checks actions:
 *
 *  - [x] Print root metadata from images repository
 */
TEST_F(AktualizrInfoTest, PrintImageRootMetadata) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  Json::Value images_root_json;
  images_root_json["key-001"] = "value-002";

  std::string images_root = Utils::jsonToStr(images_root_json);
  db_storage_->storeRoot(images_root, Uptane::RepositoryType::Image(), Uptane::Version(1));
  db_storage_->storeRoot(images_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  SpawnProcess(std::vector<std::string>{"--images-root"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("image root.json content:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(images_root), std::string::npos);
}

/**
 * Verifies aktualizr-info output of targets metadata from the image repository
 *
 * Checks actions:
 *
 *  - [x] Print targets metadata from images repository
 */
TEST_F(AktualizrInfoTest, PrintImageTargetsMetadata) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  Json::Value images_root_json;
  images_root_json["key-001"] = "value-002";

  std::string images_root = Utils::jsonToStr(images_root_json);
  db_storage_->storeRoot(images_root, Uptane::RepositoryType::Image(), Uptane::Version(1));
  db_storage_->storeRoot(images_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  Json::Value image_targets_json;
  image_targets_json["key-004"] = "value-005";
  std::string image_targets_str = Utils::jsonToStr(image_targets_json);
  db_storage_->storeNonRoot(image_targets_str, Uptane::RepositoryType::Image(), Uptane::Role::Targets());

  SpawnProcess(std::vector<std::string>{"--images-target"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("image targets.json content:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(image_targets_str), std::string::npos);
}

/**
 * Verifies aktualizr-info output of a root metadata from the director repository
 *
 * Checks actions:
 *
 *  - [x] Print root metadata from director repository
 */
TEST_F(AktualizrInfoTest, PrintDirectorRootMetadata) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  Json::Value director_root_json;
  director_root_json["key-002"] = "value-003";

  std::string director_root = Utils::jsonToStr(director_root_json);
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  SpawnProcess(std::vector<std::string>{"--director-root"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("director root.json content:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(director_root), std::string::npos);
}

/**
 * Verifies aktualizr-info output of targets' metadata from the director repository
 *
 * Checks actions:
 *
 *  - [x] Print targets metadata from director repository
 */
TEST_F(AktualizrInfoTest, PrintDirectorTargetsMetadata) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  Json::Value director_root_json;
  director_root_json["key-002"] = "value-003";

  std::string director_root = Utils::jsonToStr(director_root_json);
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  Json::Value director_targets_json;
  director_targets_json["key-004"] = "value-005";
  std::string director_targets_str = Utils::jsonToStr(director_targets_json);
  db_storage_->storeNonRoot(director_targets_str, Uptane::RepositoryType::Director(), Uptane::Role::Targets());

  SpawnProcess(std::vector<std::string>{"--director-target"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("director targets.json content:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(director_targets_str), std::string::npos);
}

/**
 * Verifies aktualizr-info output of the primary ECU keys
 *
 * Checks actions:
 *
 *  - [x] Print primary ECU keys
 */
TEST_F(AktualizrInfoTest, PrintPrimaryEcuKeys) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  const std::string public_key = "public-key-1dc766fe-136d-4c6c-bdf4-daa79c49b3c8";
  const std::string private_key = "private-key-5cb805f1-859f-48b1-b787-8055d39b6c5f";
  db_storage_->storePrimaryKeys(public_key, private_key);

  SpawnProcess(std::vector<std::string>{"--ecu-keys"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("Public key:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(public_key), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("Private key:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(private_key), std::string::npos);
}

/**
 * Verifies aktualizr-info output of TLS credentials
 *
 * Checks actions:
 *
 *  - [x] Print TLS credentials
 */
TEST_F(AktualizrInfoTest, PrintTlsCredentials) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  const std::string ca = "ca-ee532748-8837-44f5-9afb-08ba9f534fec";
  const std::string cert = "cert-74de9408-aab8-40b1-8301-682fd39db7b9";
  const std::string private_key = "private-key-39ba4622-db16-4c72-99ed-9e4abfece68b";

  db_storage_->storeTlsCreds(ca, cert, private_key);

  SpawnProcess(std::vector<std::string>{"--tls-creds"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("Root CA certificate:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(ca), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("Client certificate:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(cert), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("Client private key:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(private_key), std::string::npos);
}

/**
 * Verifies aktualizr-info output of the primary ECU's current and pending versions
 *
 * Checks actions:
 *
 *  - [x] Print primary ECU current and pending versions
 */
TEST_F(AktualizrInfoTest, PrintPrimaryEcuCurrentAndPendingVersions) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  const std::string current_ecu_version = "639a4e39-e6ba-4832-ace4-8b12cf20d562";
  const std::string pending_ecu_version = "9636753d-2a09-4c80-8b25-64b2c2d0c4df";

  db_storage_->savePrimaryInstalledVersion(
      {"update.bin", {{Uptane::Hash::Type::kSha256, current_ecu_version}}, 1, "corrid"},
      InstalledVersionUpdateMode::kCurrent);
  db_storage_->savePrimaryInstalledVersion(
      {"update-01.bin", {{Uptane::Hash::Type::kSha256, pending_ecu_version}}, 1, "corrid-01"},
      InstalledVersionUpdateMode::kPending);

  SpawnProcess();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("Current primary ecu running version: " + current_ecu_version),
            std::string::npos);
  EXPECT_NE(aktualizr_info_output.find("Pending primary ecu version: " + pending_ecu_version), std::string::npos);
}

/**
 * Verifies aktualizr-info output of the primary ECU's current and pending versions negative test
 *
 * Checks actions:
 *
 *  - [x] Print primary ECU current and pending versions
 */
TEST_F(AktualizrInfoTest, PrintPrimaryEcuCurrentAndPendingVersionsNegative) {
  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();

  const std::string pending_ecu_version = "9636753d-2a09-4c80-8b25-64b2c2d0c4df";

  SpawnProcess();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_id), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("No currently running version on primary ecu"), std::string::npos);
  EXPECT_EQ(aktualizr_info_output.find("Pending primary ecu version:"), std::string::npos);

  db_storage_->savePrimaryInstalledVersion(
      {"update-01.bin", {{Uptane::Hash::Type::kSha256, pending_ecu_version}}, 1, "corrid-01"},
      InstalledVersionUpdateMode::kPending);

  SpawnProcess();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find("No currently running version on primary ecu"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find("Pending primary ecu version: " + pending_ecu_version), std::string::npos);

  db_storage_->savePrimaryInstalledVersion(
      {"update-01.bin", {{Uptane::Hash::Type::kSha256, pending_ecu_version}}, 1, "corrid-01"},
      InstalledVersionUpdateMode::kCurrent);

  SpawnProcess();
  ASSERT_FALSE(aktualizr_info_output.empty());

  // pending ecu version became the current now
  EXPECT_NE(aktualizr_info_output.find("Current primary ecu running version: " + pending_ecu_version),
            std::string::npos);
  EXPECT_EQ(aktualizr_info_output.find("Pending primary ecu version:"), std::string::npos);
}

/**
 *  Print device name only for scripting purposes.
 */
TEST_F(AktualizrInfoTest, PrintDeviceNameOnly) {
  Json::Value meta_root;
  std::string director_root = Utils::jsonToStr(meta_root);

  db_storage_->storeEcuSerials({{Uptane::EcuSerial(primary_ecu_serial), Uptane::HardwareIdentifier(primary_ecu_id)}});
  db_storage_->storeEcuRegistered();
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  SpawnProcess(std::vector<std::string>{"--name-only", "--loglevel", "4"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_EQ(aktualizr_info_output, device_id + "\n");
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
