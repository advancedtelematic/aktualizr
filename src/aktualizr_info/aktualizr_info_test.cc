#include <gtest/gtest.h>

#include <boost/asio/io_service.hpp>
#include <boost/process.hpp>
#include <libaktualizr/config.h>
#include <libaktualizr/utils.h>

#include "storage/sqlstorage.h"
#include "test_utils.h"

constexpr char warning_no_meta_data[] = "Metadata is not available\n";

class AktualizrInfoTest : public ::testing::Test {
 protected:
  AktualizrInfoTest() : test_conf_file_{test_dir_ / "conf.toml"}, test_db_file_{test_dir_ / "sql.db"} {
    config_.pacman.type = PACKAGE_MANAGER_NONE;
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
    primary_ecu_serial = Uptane::EcuSerial("82697cac-f54c-40ea-a8f2-76c203b7bf2f");
    primary_hw_id = Uptane::HardwareIdentifier("primary-hdwr-e96c08e0-38a0-4903-a021-143cf5427bc9");

    db_storage_->storeDeviceId(device_id);
  }

  virtual void TearDown() {}

  class AktualizrInfoProcess : public Process {
   public:
    AktualizrInfoProcess(AktualizrInfoTest& test_ctx, const boost::filesystem::path& conf_file)
        : Process("./aktualizr-info"), test_ctx_{test_ctx}, conf_file_{conf_file} {}
    virtual ~AktualizrInfoProcess() {}

    void run(const std::vector<std::string> args = {}) {
      std::vector<std::string> all_args = {"-c", conf_file_.string()};

      if (args.size() > 0) {
        all_args.insert(all_args.end(), args.begin(), args.end());
      }

      test_ctx_.aktualizr_info_output.clear();
      Process::run(all_args);
      ASSERT_EQ(lastExitCode(), EXIT_SUCCESS);
      test_ctx_.aktualizr_info_output = lastStdOut();
    }

   private:
    AktualizrInfoTest& test_ctx_;
    const boost::filesystem::path conf_file_;
  };

 protected:
  TemporaryDirectory test_dir_;
  boost::filesystem::path test_conf_file_;
  boost::filesystem::path test_db_file_;

  Config config_;
  std::shared_ptr<INvStorage> db_storage_;

  std::string aktualizr_info_output;
  AktualizrInfoProcess aktualizr_info_process_{*this, test_conf_file_};

  std::string device_id;
  Uptane::EcuSerial primary_ecu_serial = Uptane::EcuSerial::Unknown();
  Uptane::HardwareIdentifier primary_hw_id = Uptane::HardwareIdentifier::Unknown();
};

/**
 * Verifies an output of the aktualizr-info in a positive case when
 * there are both Primary and Secondary present and a device is provisioned
 * and metadata are fetched from a server
 *
 * Checks actions:
 *
 *  - [x] Print device ID
 *  - [x] Print Primary ECU serial
 *  - [x] Print Primary ECU hardware ID
 *  - [x] Print Secondary ECU serials
 *  - [x] Print Secondary ECU hardware IDs
 *  - [x] Print provisioning status, if provisioned
 *  - [x] Print whether metadata has been fetched from the server, if they were fetched
 */
TEST_F(AktualizrInfoTest, PrintPrimaryAndSecondaryInfo) {
  const Uptane::EcuSerial secondary_ecu_serial{"c6998d3e-2a68-4ac2-817e-4ea6ef87d21f"};
  const Uptane::HardwareIdentifier secondary_hw_id{"secondary-hdwr-af250269-bd6f-4148-9426-4101df7f613a"};
  const std::string provisioning_status = "Provisioned on server: yes";
  const std::string fetched_metadata = "Fetched metadata: yes";

  Json::Value meta_root;
  std::string director_root = Utils::jsonToStr(meta_root);

  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}, {secondary_ecu_serial, secondary_hw_id}});
  db_storage_->storeEcuRegistered();
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  aktualizr_info_process_.run();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial.ToString()), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_hw_id.ToString()), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find(secondary_ecu_serial.ToString()), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(secondary_hw_id.ToString()), std::string::npos);

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

  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});

  aktualizr_info_process_.run();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(provisioning_status), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(fetched_metadata), std::string::npos);
}

/**
 * Verifies an output of miscofigured Secondary ECUs
 *
 * Checks actions:
 *
 * - [x] Print Secondary ECUs no longer accessible (miscofigured: old)
 * - [x] Print Secondary ECUs registered after provisioning (not registered)
 */
TEST_F(AktualizrInfoTest, PrintSecondaryNotRegisteredOrRemoved) {
  const std::string provisioning_status = "Provisioned on server: yes";

  const Uptane::EcuSerial secondary_ecu_serial{"c6998d3e-2a68-4ac2-817e-4ea6ef87d21f"};
  const Uptane::HardwareIdentifier secondary_hw_id{"secondary-hdwr-af250269-bd6f-4148-9426-4101df7f613a"};

  const Uptane::EcuSerial secondary_ecu_serial_not_reg{"18b018a1-fdda-4461-a281-42237256cc2f"};
  const Uptane::HardwareIdentifier secondary_hw_id_not_reg{"secondary-hdwr-cbce3a7a-7cbb-4da4-9fff-8e10e5c3de98"};

  const Uptane::EcuSerial secondary_ecu_serial_old{"c2191c12-7298-4be3-b781-d223dac7f75e"};
  const Uptane::HardwareIdentifier secondary_hw_id_old{"secondary-hdwr-0ded1c51-d280-49c3-a92b-7ff2c2e91d8c"};

  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}, {secondary_ecu_serial, secondary_hw_id}});
  db_storage_->storeEcuRegistered();

  db_storage_->saveMisconfiguredEcu({secondary_ecu_serial_not_reg, secondary_hw_id_not_reg, EcuState::kUnused});
  db_storage_->saveMisconfiguredEcu({secondary_ecu_serial_old, secondary_hw_id_old, EcuState::kOld});

  aktualizr_info_process_.run();
  ASSERT_FALSE(aktualizr_info_output.empty());

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
 * Verifies aktualizr-info output of a Root metadata from the Image repository
 *
 * Checks actions:
 *
 *  - [x] Print Root metadata from Image repository
 */
TEST_F(AktualizrInfoTest, PrintImageRootMetadata) {
  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});
  db_storage_->storeEcuRegistered();

  Json::Value image_root_json;
  image_root_json["key-001"] = "value-002";

  std::string image_root = Utils::jsonToStr(image_root_json);
  db_storage_->storeRoot(image_root, Uptane::RepositoryType::Image(), Uptane::Version(1));
  db_storage_->storeRoot(image_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  aktualizr_info_process_.run(std::vector<std::string>{"--images-root"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(image_root), std::string::npos);
}

/**
 * Verifies aktualizr-info output of Targets metadata from the Image repository
 *
 * Checks actions:
 *
 *  - [x] Print Targets metadata from Image repository
 */
TEST_F(AktualizrInfoTest, PrintImageTargetsMetadata) {
  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});
  db_storage_->storeEcuRegistered();

  Json::Value image_root_json;
  image_root_json["key-001"] = "value-002";

  std::string image_root = Utils::jsonToStr(image_root_json);
  db_storage_->storeRoot(image_root, Uptane::RepositoryType::Image(), Uptane::Version(1));
  db_storage_->storeRoot(image_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  Json::Value image_targets_json;
  image_targets_json["key-004"] = "value-005";
  std::string image_targets_str = Utils::jsonToStr(image_targets_json);
  db_storage_->storeNonRoot(image_targets_str, Uptane::RepositoryType::Image(), Uptane::Role::Targets());

  aktualizr_info_process_.run({"--images-target"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(image_targets_str), std::string::npos);
}

/**
 * Verifies aktualizr-info output of a Root metadata from the Director repository
 *
 * Checks actions:
 *
 *  - [x] Print Root metadata from Director repository
 */
TEST_F(AktualizrInfoTest, PrintDirectorRootMetadata) {
  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});
  db_storage_->storeEcuRegistered();

  Json::Value director_root_json;
  director_root_json["key-002"] = "value-003";

  std::string director_root = Utils::jsonToStr(director_root_json);
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  aktualizr_info_process_.run({"--director-root"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(director_root), std::string::npos);
}

/**
 * Verifies aktualizr-info output of Targets metadata from the Director repository
 *
 * Checks actions:
 *
 *  - [x] Print Targets metadata from Director repository
 */
TEST_F(AktualizrInfoTest, PrintDirectorTargetsMetadata) {
  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});
  db_storage_->storeEcuRegistered();

  Json::Value director_root_json;
  director_root_json["key-002"] = "value-003";

  std::string director_root = Utils::jsonToStr(director_root_json);
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  Json::Value director_targets_json;
  director_targets_json["key-004"] = "value-005";
  std::string director_targets_str = Utils::jsonToStr(director_targets_json);
  db_storage_->storeNonRoot(director_targets_str, Uptane::RepositoryType::Director(), Uptane::Role::Targets());

  aktualizr_info_process_.run({"--director-target"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(director_targets_str), std::string::npos);
}

/**
 * Verifies aktualizr-info output of the Primary ECU keys
 *
 * Checks actions:
 *
 *  - [x] Print Primary ECU keys
 *  - [x] Print ECU public key
 *  - [x] Print ECU private key
 */
TEST_F(AktualizrInfoTest, PrintPrimaryEcuKeys) {
  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});
  db_storage_->storeEcuRegistered();

  const std::string public_key = "public-key-1dc766fe-136d-4c6c-bdf4-daa79c49b3c8";
  const std::string private_key = "private-key-5cb805f1-859f-48b1-b787-8055d39b6c5f";
  db_storage_->storePrimaryKeys(public_key, private_key);

  aktualizr_info_process_.run({"--ecu-keys"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find("Public key:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(public_key), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("Private key:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(private_key), std::string::npos);

  aktualizr_info_process_.run({"--ecu-pub-key"});
  ASSERT_FALSE(aktualizr_info_output.empty());
  EXPECT_NE(aktualizr_info_output.find(public_key), std::string::npos);

  aktualizr_info_process_.run({"--ecu-prv-key"});
  ASSERT_FALSE(aktualizr_info_output.empty());
  EXPECT_NE(aktualizr_info_output.find(private_key), std::string::npos);
}

/**
 * Verifies aktualizr-info output of TLS credentials
 *
 * Checks actions:
 *
 *  - [x] Print TLS credentials
 *  - [x] Print TLS Root CA
 *  - [x] Print TLS client certificate
 *  - [x] Print TLS client private key
 */
TEST_F(AktualizrInfoTest, PrintTlsCredentials) {
  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});
  db_storage_->storeEcuRegistered();

  const std::string ca = "ca-ee532748-8837-44f5-9afb-08ba9f534fec";
  const std::string cert = "cert-74de9408-aab8-40b1-8301-682fd39db7b9";
  const std::string private_key = "private-key-39ba4622-db16-4c72-99ed-9e4abfece68b";

  db_storage_->storeTlsCreds(ca, cert, private_key);

  aktualizr_info_process_.run({"--tls-creds"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find("Root CA certificate:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(ca), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("Client certificate:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(cert), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("Client private key:"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(private_key), std::string::npos);

  aktualizr_info_process_.run({"--tls-root-ca"});
  ASSERT_FALSE(aktualizr_info_output.empty());
  EXPECT_NE(aktualizr_info_output.find(ca), std::string::npos);

  aktualizr_info_process_.run({"--tls-cert"});
  ASSERT_FALSE(aktualizr_info_output.empty());
  EXPECT_NE(aktualizr_info_output.find(cert), std::string::npos);

  aktualizr_info_process_.run({"--tls-prv-key"});
  ASSERT_FALSE(aktualizr_info_output.empty());
  EXPECT_NE(aktualizr_info_output.find(private_key), std::string::npos);
}

/**
 * Verifies aktualizr-info output of the Primary ECU's current and pending versions
 *
 * Checks actions:
 *
 *  - [x] Print Primary ECU's current and pending versions
 */
TEST_F(AktualizrInfoTest, PrintPrimaryEcuCurrentAndPendingVersions) {
  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});
  db_storage_->storeEcuRegistered();

  const std::string current_ecu_version = "639a4e39-e6ba-4832-ace4-8b12cf20d562";
  const std::string pending_ecu_version = "9636753d-2a09-4c80-8b25-64b2c2d0c4df";

  Uptane::EcuMap ecu_map{{primary_ecu_serial, primary_hw_id}};
  db_storage_->savePrimaryInstalledVersion(
      {"update.bin", ecu_map, {{Hash::Type::kSha256, current_ecu_version}}, 1, "corrid"},
      InstalledVersionUpdateMode::kCurrent);
  db_storage_->savePrimaryInstalledVersion(
      {"update-01.bin", ecu_map, {{Hash::Type::kSha256, pending_ecu_version}}, 1, "corrid-01"},
      InstalledVersionUpdateMode::kPending);

  aktualizr_info_process_.run();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find("Current Primary ECU running version: " + current_ecu_version),
            std::string::npos);
  EXPECT_NE(aktualizr_info_output.find("Pending Primary ECU version: " + pending_ecu_version), std::string::npos);
}

/**
 * Verifies aktualizr-info output of the Primary ECU's current and pending versions negative test
 *
 * Checks actions:
 *
 *  - [x] Print Primary ECU's current and pending versions
 */
TEST_F(AktualizrInfoTest, PrintPrimaryEcuCurrentAndPendingVersionsNegative) {
  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});
  db_storage_->storeEcuRegistered();

  const std::string pending_ecu_version = "9636753d-2a09-4c80-8b25-64b2c2d0c4df";

  aktualizr_info_process_.run();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(device_id), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_ecu_serial.ToString()), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find(primary_hw_id.ToString()), std::string::npos);

  EXPECT_NE(aktualizr_info_output.find("No currently running version on Primary ECU"), std::string::npos);
  EXPECT_EQ(aktualizr_info_output.find("Pending Primary ECU version:"), std::string::npos);

  Uptane::EcuMap ecu_map{{primary_ecu_serial, primary_hw_id}};
  db_storage_->savePrimaryInstalledVersion(
      {"update-01.bin", ecu_map, {{Hash::Type::kSha256, pending_ecu_version}}, 1, "corrid-01"},
      InstalledVersionUpdateMode::kPending);

  aktualizr_info_process_.run();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find("No currently running version on Primary ECU"), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find("Pending Primary ECU version: " + pending_ecu_version), std::string::npos);

  db_storage_->savePrimaryInstalledVersion(
      {"update-01.bin", ecu_map, {{Hash::Type::kSha256, pending_ecu_version}}, 1, "corrid-01"},
      InstalledVersionUpdateMode::kCurrent);

  aktualizr_info_process_.run();
  ASSERT_FALSE(aktualizr_info_output.empty());

  // pending ECU version became the current now
  EXPECT_NE(aktualizr_info_output.find("Current Primary ECU running version: " + pending_ecu_version),
            std::string::npos);
  EXPECT_EQ(aktualizr_info_output.find("Pending Primary ECU version:"), std::string::npos);
}

/**
 * Verifies aktualizr-info output of Secondary ECU's current and pending versions
 *
 * Checks actions:
 *
 *  - [x] Print Secondary ECU current and pending versions
 */
TEST_F(AktualizrInfoTest, PrintSecondaryEcuCurrentAndPendingVersions) {
  const Uptane::EcuSerial secondary_ecu_serial{"c6998d3e-2a68-4ac2-817e-4ea6ef87d21f"};
  const Uptane::HardwareIdentifier secondary_hw_id{"secondary-hdwr-af250269-bd6f-4148-9426-4101df7f613a"};
  const std::string secondary_ecu_filename = "secondary.file";
  const std::string secondary_ecu_filename_update = "secondary.file.update";
  const std::string current_ecu_version = "639a4e39-e6ba-4832-ace4-8b12cf20d562";
  const std::string pending_ecu_version = "9636753d-2a09-4c80-8b25-64b2c2d0c4df";

  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}, {secondary_ecu_serial, secondary_hw_id}});
  db_storage_->storeEcuRegistered();

  Uptane::EcuMap ecu_map{{secondary_ecu_serial, secondary_hw_id}};
  db_storage_->saveInstalledVersion(secondary_ecu_serial.ToString(),
                                    {secondary_ecu_filename, ecu_map, {{Hash::Type::kSha256, current_ecu_version}}, 1},
                                    InstalledVersionUpdateMode::kCurrent);

  db_storage_->saveInstalledVersion(
      secondary_ecu_serial.ToString(),
      {secondary_ecu_filename_update, ecu_map, {{Hash::Type::kSha256, pending_ecu_version}}, 1},
      InstalledVersionUpdateMode::kPending);

  aktualizr_info_process_.run();
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find("installed image hash: " + current_ecu_version), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find("installed image filename: " + secondary_ecu_filename), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find("pending image hash: " + pending_ecu_version), std::string::npos);
  EXPECT_NE(aktualizr_info_output.find("pending image filename: " + secondary_ecu_filename_update), std::string::npos);

  // negative test, no any installed images
  db_storage_->clearInstalledVersions();
  db_storage_->clearEcuSerials();
  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}, {secondary_ecu_serial, secondary_hw_id}});

  aktualizr_info_process_.run();
  ASSERT_FALSE(aktualizr_info_output.empty());
  EXPECT_NE(aktualizr_info_output.find("no details about installed nor pending images"), std::string::npos);
}

/**
 *  Print device name only for scripting purposes.
 */
TEST_F(AktualizrInfoTest, PrintDeviceNameOnly) {
  Json::Value meta_root;
  std::string director_root = Utils::jsonToStr(meta_root);

  db_storage_->storeEcuSerials({{primary_ecu_serial, primary_hw_id}});
  db_storage_->storeEcuRegistered();
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  aktualizr_info_process_.run({"--name-only"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_EQ(aktualizr_info_output, device_id + "\n");
}

/**
 * Verifies delegations metadata fetching and output
 *
 * * Checks actions:
 *
 *  - [x] Print delegations
 */
TEST_F(AktualizrInfoTest, PrintDelegations) {
  auto gen_and_store_delegations = [](std::shared_ptr<INvStorage>& db_storage,
                                      std::vector<std::pair<Uptane::Role, std::string> >& delegation_records) {
    unsigned indx = 0;
    for (auto& delegation_rec : delegation_records) {
      const std::string indx_str = std::to_string(indx);
      const std::string delegation_role_val = "delegation_role_" + indx_str;

      Json::Value delegation;
      std::string delegation_val_str =
          Utils::jsonToStr((delegation["delegation_value_key_" + indx_str] = "delegation_value_" + indx_str));

      delegation_rec.first = Uptane::Role::Delegation(delegation_role_val);
      delegation_rec.second = delegation_val_str;
      db_storage->storeDelegation(delegation_val_str, Uptane::Role::Delegation(delegation_role_val));

      ++indx;
    }
  };

  auto verify_delegations = [](const std::string& output_str,
                               std::vector<std::pair<Uptane::Role, std::string> >& delegation_records) {
    for (auto& delegation_rec : delegation_records) {
      ASSERT_NE(output_str.find(delegation_rec.first.ToString()), std::string::npos);
      ASSERT_NE(output_str.find(delegation_rec.second), std::string::npos);
    }
  };

  // aktualizr-info won't print anything if Director Root metadata are not stored in the DB
  db_storage_->storeRoot(Utils::jsonToStr(Json::Value()), Uptane::RepositoryType::Director(), Uptane::Version(1));

  // case 0: no delegations in the DB
  {
    aktualizr_info_process_.run({"--delegation"});
    ASSERT_FALSE(aktualizr_info_output.empty());
    EXPECT_NE(aktualizr_info_output.find("Delegations are not present"), std::string::npos);
  }

  // case 1: there is one delegation metadata record in the DB
  {
    std::vector<std::pair<Uptane::Role, std::string> > delegation_records{1, {Uptane::Role::Delegation(""), ""}};
    gen_and_store_delegations(db_storage_, delegation_records);

    aktualizr_info_process_.run({"--delegation"});
    ASSERT_FALSE(aktualizr_info_output.empty());

    verify_delegations(aktualizr_info_output, delegation_records);
  }

  db_storage_->clearDelegations();

  // case 2: there are more than one delegation metadata records in the DB
  {
    std::vector<std::pair<Uptane::Role, std::string> > delegation_records{3, {Uptane::Role::Delegation(""), ""}};
    gen_and_store_delegations(db_storage_, delegation_records);

    aktualizr_info_process_.run({"--delegation"});
    ASSERT_FALSE(aktualizr_info_output.empty());

    verify_delegations(aktualizr_info_output, delegation_records);
  }
}

/**
 * Verifies aktualizr-info output of Snapshot metadata from Image repository
 *
 * Checks actions:
 *
 *  - [x] Print Snapshot metadata from Image repository
 */
TEST_F(AktualizrInfoTest, PrintImageSnapshotMetadata) {
  Json::Value director_root_json;
  director_root_json["key-002"] = "value-003";
  std::string director_root = Utils::jsonToStr(director_root_json);
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  Json::Value meta_snapshot;
  meta_snapshot["signed"]["_type"] = "Snapshot";
  meta_snapshot["signed"]["expires"] = "2038-01-19T03:14:06Z";
  meta_snapshot["signed"]["version"] = "2";
  std::string image_snapshot = Utils::jsonToStr(meta_snapshot);
  db_storage_->storeNonRoot(image_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());

  aktualizr_info_process_.run({"--images-snapshot"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(image_snapshot), std::string::npos);
}

/**
 * Verifies aktualizr-info output of Timestamp metadata from the Image repository
 *
 * Checks actions:
 *
 *  - [x] Print Timestamp metadata from Image repository
 */
TEST_F(AktualizrInfoTest, PrintImageTimestampMetadata) {
  Json::Value director_root_json;
  director_root_json["key-002"] = "value-003";
  std::string director_root = Utils::jsonToStr(director_root_json);
  db_storage_->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));

  Json::Value meta_timestamp;
  meta_timestamp["signed"]["_type"] = "Timestamp";
  meta_timestamp["signed"]["expires"] = "2038-01-19T03:14:06Z";
  std::string image_timestamp = Utils::jsonToStr(meta_timestamp);
  db_storage_->storeNonRoot(image_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());

  aktualizr_info_process_.run({"--images-timestamp"});
  ASSERT_FALSE(aktualizr_info_output.empty());

  EXPECT_NE(aktualizr_info_output.find(image_timestamp), std::string::npos);
}

/**
 * Verifies aktualizr-info output when metadata is not present
 *
 * Check actions:
 *  - [x] Print appropriate message if the metadata does not exist in storage..
 */
TEST_F(AktualizrInfoTest, PrintMetadataWarnibg) {
  db_storage_->clearMetadata();

  const std::vector<std::string> args = {"--images-root",     "--images-target",   "--delegation",
                                         "--director-root",   "--director-target", "--images-snapshot",
                                         "--images-timestamp"};

  for (auto arg : args) {
    aktualizr_info_process_.run({arg});
    ASSERT_FALSE(aktualizr_info_output.empty());
    EXPECT_NE(aktualizr_info_output.find(std::string(warning_no_meta_data)), std::string::npos);
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
