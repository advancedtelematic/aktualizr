#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <boost/filesystem.hpp>

#include "logging/logging.h"
#include "storage/sqlstorage.h"
#include "utilities/types.h"
#include "utilities/utils.h"

StorageType current_storage_type{StorageType::kSqlite};

std::unique_ptr<INvStorage> Storage(const boost::filesystem::path &dir) {
  StorageConfig storage_config;
  storage_config.type = current_storage_type;
  storage_config.path = dir;

  if (storage_config.type == StorageType::kSqlite) {
    return std::unique_ptr<INvStorage>(new SQLStorage(storage_config, false));
  } else {
    throw std::runtime_error("Invalid config type");
  }
}

StorageConfig MakeConfig(StorageType type, const boost::filesystem::path &storage_dir) {
  StorageConfig config;

  config.type = type;
  if (config.type == StorageType::kSqlite) {
    config.sqldb_path = storage_dir / "test.db";
  } else {
    throw std::runtime_error("Invalid config type");
  }
  return config;
}

/* Load and store Primary keys. */
TEST(storage, load_store_primary_keys) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  storage->storePrimaryKeys("", "");
  storage->storePrimaryKeys("pr_public", "pr_private");

  std::string pubkey;
  std::string privkey;

  EXPECT_TRUE(storage->loadPrimaryKeys(&pubkey, &privkey));
  EXPECT_EQ(pubkey, "pr_public");
  EXPECT_EQ(privkey, "pr_private");
  storage->clearPrimaryKeys();
  EXPECT_FALSE(storage->loadPrimaryKeys(nullptr, nullptr));
}

/* Load and store TLS credentials. */
TEST(storage, load_store_tls) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  storage->storeTlsCreds("", "", "");
  storage->storeTlsCreds("ca", "cert", "priv");
  std::string ca;
  std::string cert;
  std::string priv;

  EXPECT_TRUE(storage->loadTlsCreds(&ca, &cert, &priv));

  EXPECT_EQ(ca, "ca");
  EXPECT_EQ(cert, "cert");
  EXPECT_EQ(priv, "priv");
  storage->clearTlsCreds();
  EXPECT_FALSE(storage->loadTlsCreds(nullptr, nullptr, nullptr));
}

/* Load and store Uptane metadata. */
TEST(storage, load_store_metadata) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  Json::Value root_json;
  root_json["_type"] = "Root";
  root_json["consistent_snapshot"] = false;
  root_json["expires"] = "2038-01-19T03:14:06Z";
  root_json["keys"]["firstid"]["keytype"] = "ed25519";
  root_json["keys"]["firstid"]["keyval"]["public"] = "firstval";
  root_json["keys"]["secondid"]["keytype"] = "ed25519";
  root_json["keys"]["secondid"]["keyval"]["public"] = "secondval";

  root_json["roles"]["root"]["threshold"] = 1;
  root_json["roles"]["root"]["keyids"][0] = "firstid";
  root_json["roles"]["snapshot"]["threshold"] = 1;
  root_json["roles"]["snapshot"]["keyids"][0] = "firstid";
  root_json["roles"]["targets"]["threshold"] = 1;
  root_json["roles"]["targets"]["keyids"][0] = "firstid";
  root_json["roles"]["timestamp"]["threshold"] = 1;
  root_json["roles"]["timestamp"]["keyids"][0] = "firstid";

  Json::Value meta_root;
  meta_root["signed"] = root_json;
  std::string director_root = Utils::jsonToStr(meta_root);
  std::string image_root = Utils::jsonToStr(meta_root);

  Json::Value targets_json;
  targets_json["_type"] = "Targets";
  targets_json["expires"] = "2038-01-19T03:14:06Z";
  targets_json["targets"]["file1"]["custom"]["ecu_identifier"] = "ecu1";
  targets_json["targets"]["file1"]["custom"]["hardware_identifier"] = "hw1";
  targets_json["targets"]["file1"]["hashes"]["sha256"] = "12ab";
  targets_json["targets"]["file1"]["length"] = 1;
  targets_json["targets"]["file2"]["custom"]["ecu_identifier"] = "ecu2";
  targets_json["targets"]["file2"]["custom"]["hardware_identifier"] = "hw2";
  targets_json["targets"]["file2"]["hashes"]["sha512"] = "12ab";
  targets_json["targets"]["file2"]["length"] = 11;

  Json::Value meta_targets;
  meta_targets["signed"] = targets_json;
  std::string director_targets = Utils::jsonToStr(meta_targets);
  std::string image_targets = Utils::jsonToStr(meta_targets);

  Json::Value timestamp_json;
  timestamp_json["signed"]["_type"] = "Timestamp";
  timestamp_json["signed"]["expires"] = "2038-01-19T03:14:06Z";
  std::string image_timestamp = Utils::jsonToStr(timestamp_json);

  Json::Value snapshot_json;
  snapshot_json["_type"] = "Snapshot";
  snapshot_json["expires"] = "2038-01-19T03:14:06Z";
  snapshot_json["meta"]["root.json"]["version"] = 1;
  snapshot_json["meta"]["targets.json"]["version"] = 2;
  snapshot_json["meta"]["timestamp.json"]["version"] = 3;
  snapshot_json["meta"]["snapshot.json"]["version"] = 4;

  Json::Value meta_snapshot;
  meta_snapshot["signed"] = snapshot_json;
  std::string image_snapshot = Utils::jsonToStr(meta_snapshot);

  storage->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));
  storage->storeNonRoot(director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
  storage->storeRoot(image_root, Uptane::RepositoryType::Image(), Uptane::Version(1));
  storage->storeNonRoot(image_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets());
  storage->storeNonRoot(image_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());
  storage->storeNonRoot(image_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());

  std::string loaded_director_root;
  std::string loaded_director_targets;
  std::string loaded_image_root;
  std::string loaded_image_targets;
  std::string loaded_image_timestamp;
  std::string loaded_image_snapshot;

  EXPECT_TRUE(storage->loadLatestRoot(&loaded_director_root, Uptane::RepositoryType::Director()));
  EXPECT_TRUE(
      storage->loadNonRoot(&loaded_director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));
  EXPECT_TRUE(storage->loadLatestRoot(&loaded_image_root, Uptane::RepositoryType::Image()));
  EXPECT_TRUE(storage->loadNonRoot(&loaded_image_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets()));
  EXPECT_TRUE(
      storage->loadNonRoot(&loaded_image_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));
  EXPECT_TRUE(storage->loadNonRoot(&loaded_image_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()));
  EXPECT_EQ(director_root, loaded_director_root);
  EXPECT_EQ(director_targets, loaded_director_targets);
  EXPECT_EQ(image_root, loaded_image_root);
  EXPECT_EQ(image_targets, loaded_image_targets);
  EXPECT_EQ(image_timestamp, loaded_image_timestamp);
  EXPECT_EQ(image_snapshot, loaded_image_snapshot);

  storage->clearNonRootMeta(Uptane::RepositoryType::Director());
  storage->clearNonRootMeta(Uptane::RepositoryType::Image());
  EXPECT_FALSE(
      storage->loadNonRoot(&loaded_director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));
  EXPECT_FALSE(
      storage->loadNonRoot(&loaded_image_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));
}

/* Load and store Uptane roots. */
TEST(storage, load_store_root) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  Json::Value root_json;
  root_json["_type"] = "Root";
  root_json["consistent_snapshot"] = false;
  root_json["expires"] = "2038-01-19T03:14:06Z";
  root_json["keys"]["firstid"]["keytype"] = "ed25519";
  root_json["keys"]["firstid"]["keyval"]["public"] = "firstval";
  root_json["keys"]["secondid"]["keytype"] = "ed25519";
  root_json["keys"]["secondid"]["keyval"]["public"] = "secondval";

  root_json["roles"]["root"]["threshold"] = 1;
  root_json["roles"]["root"]["keyids"][0] = "firstid";
  root_json["roles"]["snapshot"]["threshold"] = 1;
  root_json["roles"]["snapshot"]["keyids"][0] = "firstid";
  root_json["roles"]["targets"]["threshold"] = 1;
  root_json["roles"]["targets"]["keyids"][0] = "firstid";
  root_json["roles"]["timestamp"]["threshold"] = 1;
  root_json["roles"]["timestamp"]["keyids"][0] = "firstid";

  Json::Value meta_root;
  meta_root["signed"] = root_json;

  std::string loaded_root;

  storage->storeRoot(Utils::jsonToStr(meta_root), Uptane::RepositoryType::Director(), Uptane::Version(2));
  EXPECT_TRUE(storage->loadRoot(&loaded_root, Uptane::RepositoryType::Director(), Uptane::Version(2)));
  EXPECT_EQ(Utils::jsonToStr(meta_root), loaded_root);

  EXPECT_TRUE(storage->loadLatestRoot(&loaded_root, Uptane::RepositoryType::Director()));
  EXPECT_EQ(Utils::jsonToStr(meta_root), loaded_root);
}

/* Load and store the device ID. */
TEST(storage, load_store_deviceid) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  storage->storeDeviceId("");
  storage->storeDeviceId("device_id");

  std::string device_id;

  EXPECT_TRUE(storage->loadDeviceId(&device_id));

  EXPECT_EQ(device_id, "device_id");
  storage->clearDeviceId();
  EXPECT_FALSE(storage->loadDeviceId(nullptr));
}

/* Load and store ECU serials.
 * Preserve ECU ordering between store and load calls.
 */
TEST(storage, load_store_ecu_serials) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  storage->storeEcuSerials({{Uptane::EcuSerial("a"), Uptane::HardwareIdentifier("")}});
  EcuSerials serials{{Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw")},
                     {Uptane::EcuSerial("secondary_1"), Uptane::HardwareIdentifier("secondary_hw")},
                     {Uptane::EcuSerial("secondary_2"), Uptane::HardwareIdentifier("secondary_hw")}};
  storage->storeEcuSerials(serials);

  EcuSerials serials_out;

  EXPECT_TRUE(storage->loadEcuSerials(&serials_out));

  EXPECT_EQ(serials, serials_out);
  storage->clearEcuSerials();
  EXPECT_FALSE(storage->loadEcuSerials(nullptr));
}

/* Load and store a list of misconfigured ECUs. */
TEST(storage, load_store_misconfigured_ecus) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  storage->saveMisconfiguredEcu(
      {Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw"), EcuState::kOld});

  std::vector<MisconfiguredEcu> ecus_out;

  EXPECT_TRUE(storage->loadMisconfiguredEcus(&ecus_out));

  EXPECT_EQ(ecus_out.size(), 1);
  EXPECT_EQ(ecus_out[0].serial, Uptane::EcuSerial("primary"));
  EXPECT_EQ(ecus_out[0].hardware_id, Uptane::HardwareIdentifier("primary_hw"));
  EXPECT_EQ(ecus_out[0].state, EcuState::kOld);

  storage->clearMisconfiguredEcus();
  ecus_out.clear();
  EXPECT_FALSE(storage->loadMisconfiguredEcus(&ecus_out));
}

/* Load and store a flag indicating successful registration. */
TEST(storage, load_store_ecu_registered) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  EXPECT_THROW(storage->storeEcuRegistered(), std::runtime_error);
  storage->storeDeviceId("test");
  storage->storeEcuRegistered();
  storage->storeEcuRegistered();

  EXPECT_TRUE(storage->loadEcuRegistered());

  storage->clearEcuRegistered();
  EXPECT_FALSE(storage->loadEcuRegistered());
}

/* Load and store installed versions. */
TEST(storage, load_store_installed_versions) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  // Test lazy Primary installed version: Primary ECU serial is not defined yet
  const std::vector<Hash> hashes = {
      Hash{Hash::Type::kSha256, "2561"},
      Hash{Hash::Type::kSha512, "5121"},
  };
  Uptane::EcuMap primary_ecu{{Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw")}};
  Uptane::Target t1{"update.bin", primary_ecu, hashes, 1, "corrid"};
  Json::Value custom;
  custom["version"] = 42;
  custom["foo"] = "bar";
  t1.updateCustom(custom);
  storage->savePrimaryInstalledVersion(t1, InstalledVersionUpdateMode::kCurrent);
  {
    std::vector<Uptane::Target> log;
    storage->loadPrimaryInstallationLog(&log, true);
    EXPECT_EQ(log.size(), 1);
    EXPECT_EQ(log[0].filename(), "update.bin");
  }

  EcuSerials serials{{Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw")},
                     {Uptane::EcuSerial("secondary_1"), Uptane::HardwareIdentifier("secondary_hw")},
                     {Uptane::EcuSerial("secondary_2"), Uptane::HardwareIdentifier("secondary_hw")}};
  storage->storeEcuSerials(serials);

  {
    boost::optional<Uptane::Target> current;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", &current, nullptr));
    EXPECT_FALSE(storage->hasPendingInstall());
    EXPECT_TRUE(!!current);
    EXPECT_EQ(current->filename(), "update.bin");
    EXPECT_EQ(current->sha256Hash(), "2561");
    EXPECT_EQ(current->hashes(), hashes);
    EXPECT_EQ(current->ecus(), primary_ecu);
    EXPECT_EQ(current->correlation_id(), "corrid");
    EXPECT_EQ(current->length(), 1);
    EXPECT_EQ(current->custom_data()["foo"], "bar");
    EXPECT_EQ(current->custom_data()["version"], 42);
  }

  // Set t2 as a pending version
  Uptane::Target t2{"update2.bin", primary_ecu, {Hash{Hash::Type::kSha256, "2562"}}, 2};
  storage->savePrimaryInstalledVersion(t2, InstalledVersionUpdateMode::kPending);

  {
    boost::optional<Uptane::Target> pending;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", nullptr, &pending));
    EXPECT_TRUE(!!pending);
    EXPECT_TRUE(storage->hasPendingInstall());
    EXPECT_EQ(pending->filename(), "update2.bin");
  }

  // Set t3 as the new pending
  Uptane::Target t3{"update3.bin", primary_ecu, {Hash{Hash::Type::kSha256, "2563"}}, 3};
  storage->savePrimaryInstalledVersion(t3, InstalledVersionUpdateMode::kPending);

  {
    boost::optional<Uptane::Target> pending;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", nullptr, &pending));
    EXPECT_TRUE(!!pending);
    EXPECT_TRUE(storage->hasPendingInstall());
    EXPECT_EQ(pending->filename(), "update3.bin");
  }

  // Set t3 as current: should replace the pending flag but not create a new
  // version
  storage->savePrimaryInstalledVersion(t3, InstalledVersionUpdateMode::kCurrent);
  {
    boost::optional<Uptane::Target> current;
    boost::optional<Uptane::Target> pending;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", &current, &pending));
    EXPECT_TRUE(!!current);
    EXPECT_EQ(current->filename(), "update3.bin");
    EXPECT_FALSE(!!pending);
    EXPECT_FALSE(storage->hasPendingInstall());

    std::vector<Uptane::Target> log;
    storage->loadInstallationLog("primary", &log, true);
    EXPECT_EQ(log.size(), 2);
    EXPECT_EQ(log.back().filename(), "update3.bin");
  }

  // Set t1 as current: the log should have grown even though we rolled back
  {
    storage->savePrimaryInstalledVersion(t1, InstalledVersionUpdateMode::kCurrent);
    std::vector<Uptane::Target> log;
    storage->loadInstallationLog("primary", &log, true);
    EXPECT_EQ(log.size(), 3);
    EXPECT_EQ(log.back().filename(), "update.bin");
    EXPECT_FALSE(storage->hasPendingInstall());
  }

  // Set t2 as the new pending and t3 as current afterwards: the pending flag
  // should disappear
  storage->savePrimaryInstalledVersion(t2, InstalledVersionUpdateMode::kPending);
  storage->savePrimaryInstalledVersion(t3, InstalledVersionUpdateMode::kCurrent);

  {
    boost::optional<Uptane::Target> current;
    boost::optional<Uptane::Target> pending;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", &current, &pending));
    EXPECT_TRUE(!!current);
    EXPECT_EQ(current->filename(), "update3.bin");
    EXPECT_FALSE(!!pending);
    EXPECT_FALSE(storage->hasPendingInstall());

    std::vector<Uptane::Target> log;
    storage->loadInstallationLog("primary", &log, true);
    EXPECT_EQ(log.size(), 4);
    EXPECT_EQ(log.back().filename(), "update3.bin");
    EXPECT_EQ(log[0].custom_data()["foo"], "bar");
  }

  // Add a Secondary installed version
  Uptane::EcuMap secondary_ecu{{Uptane::EcuSerial("secondary1"), Uptane::HardwareIdentifier("secondary_hw")}};
  Uptane::Target tsec{"secondary.bin", secondary_ecu, {Hash{Hash::Type::kSha256, "256s"}}, 4};
  storage->saveInstalledVersion("secondary_1", tsec, InstalledVersionUpdateMode::kCurrent);

  {
    EXPECT_TRUE(storage->loadInstalledVersions("primary", nullptr, nullptr));
    EXPECT_TRUE(storage->loadInstalledVersions("secondary_1", nullptr, nullptr));

    std::vector<Uptane::Target> log;
    storage->loadInstallationLog("secondary_1", &log, true);
    EXPECT_EQ(log.size(), 1);
    EXPECT_EQ(log.back().filename(), "secondary.bin");
  }
}

/*
 * Load and store an ECU installation result in an SQL database.
 * Load and store a device installation result in an SQL database.
 */
TEST(storage, load_store_installation_results) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  EcuSerials serials{{Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw")},
                     {Uptane::EcuSerial("secondary_1"), Uptane::HardwareIdentifier("secondary_hw")},
                     {Uptane::EcuSerial("secondary_2"), Uptane::HardwareIdentifier("secondary_hw")}};
  storage->storeEcuSerials(serials);

  storage->saveEcuInstallationResult(Uptane::EcuSerial("secondary_2"), data::InstallationResult());
  storage->saveEcuInstallationResult(Uptane::EcuSerial("primary"), data::InstallationResult());
  storage->saveEcuInstallationResult(Uptane::EcuSerial("primary"),
                                     data::InstallationResult(data::ResultCode::Numeric::kGeneralError, ""));

  std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> res;
  EXPECT_TRUE(storage->loadEcuInstallationResults(&res));
  EXPECT_EQ(res.size(), 2);
  EXPECT_EQ(res.at(0).first.ToString(), "primary");
  EXPECT_EQ(res.at(0).second.result_code.num_code, data::ResultCode::Numeric::kGeneralError);
  EXPECT_EQ(res.at(1).first.ToString(), "secondary_2");
  EXPECT_EQ(res.at(1).second.result_code.num_code, data::ResultCode::Numeric::kOk);

  storage->storeDeviceInstallationResult(data::InstallationResult(data::ResultCode::Numeric::kGeneralError, ""), "raw",
                                         "corrid");

  data::InstallationResult dev_res;
  std::string report;
  std::string correlation_id;
  EXPECT_TRUE(storage->loadDeviceInstallationResult(&dev_res, &report, &correlation_id));
  EXPECT_EQ(dev_res.result_code.num_code, data::ResultCode::Numeric::kGeneralError);
  EXPECT_EQ(report, "raw");
  EXPECT_EQ(correlation_id, "corrid");

  storage->clearInstallationResults();
  res.clear();
  EXPECT_FALSE(storage->loadEcuInstallationResults(&res));
  EXPECT_EQ(res.size(), 0);
  EXPECT_FALSE(storage->loadDeviceInstallationResult(&dev_res, &report, &correlation_id));
}

TEST(storage, downloaded_files_info) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  storage->storeTargetFilename("target1", "file1");
  storage->storeTargetFilename("target2", "file2");
  ASSERT_EQ(storage->getTargetFilename("target1"), "file1");
  ASSERT_EQ(storage->getTargetFilename("target2"), "file2");

  auto names = storage->getAllTargetNames();
  ASSERT_EQ(names.size(), 2);
  ASSERT_EQ(names.at(0), "target1");
  ASSERT_EQ(names.at(1), "target2");

  storage->deleteTargetInfo("target1");
  names = storage->getAllTargetNames();
  ASSERT_EQ(names.size(), 1);
  ASSERT_EQ(names.at(0), "target2");
}

TEST(storage, load_store_secondary_info) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  // note: this can be done before the ECU is known
  storage->saveSecondaryData(Uptane::EcuSerial("secondary_2"), "data2");

  EcuSerials serials{{Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw")},
                     {Uptane::EcuSerial("secondary_1"), Uptane::HardwareIdentifier("secondary_hw")},
                     {Uptane::EcuSerial("secondary_2"), Uptane::HardwareIdentifier("secondary_hw")}};
  storage->storeEcuSerials(serials);

  storage->saveSecondaryInfo(Uptane::EcuSerial("secondary_1"), "ip", PublicKey("key1", KeyType::kED25519));

  EXPECT_THROW(storage->saveSecondaryInfo(Uptane::EcuSerial("primary"), "ip", PublicKey("key0", KeyType::kRSA2048)),
               std::runtime_error);

  std::vector<SecondaryInfo> sec_infos;
  EXPECT_TRUE(storage->loadSecondariesInfo(&sec_infos));

  ASSERT_EQ(sec_infos.size(), 2);
  EXPECT_EQ(sec_infos[0].serial.ToString(), "secondary_1");
  EXPECT_EQ(sec_infos[0].hw_id.ToString(), "secondary_hw");
  EXPECT_EQ(sec_infos[0].type, "ip");
  EXPECT_EQ(sec_infos[0].pub_key.Value(), "key1");
  EXPECT_EQ(sec_infos[0].pub_key.Type(), KeyType::kED25519);
  EXPECT_EQ(sec_infos[1].pub_key.Type(), KeyType::kUnknown);
  EXPECT_EQ(sec_infos[1].type, "");
  EXPECT_EQ(sec_infos[1].extra, "data2");

  // test update of data
  storage->saveSecondaryInfo(Uptane::EcuSerial("secondary_1"), "ip", PublicKey("key2", KeyType::kED25519));
  storage->saveSecondaryData(Uptane::EcuSerial("secondary_1"), "data1");
  EXPECT_TRUE(storage->loadSecondariesInfo(&sec_infos));

  ASSERT_EQ(sec_infos.size(), 2);
  EXPECT_EQ(sec_infos[0].pub_key.Value(), "key2");
  EXPECT_EQ(sec_infos[0].extra, "data1");
}

/* Import keys and credentials from file into storage. */
TEST(storage, import_data) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());
  boost::filesystem::create_directories(temp_dir / "import");

  ImportConfig import_config;
  import_config.base_path = temp_dir.Path() / "import";
  import_config.uptane_private_key_path = BasedPath("private");
  import_config.uptane_public_key_path = BasedPath("public");
  import_config.tls_cacert_path = BasedPath("ca");
  import_config.tls_clientcert_path = BasedPath("cert");
  import_config.tls_pkey_path = BasedPath("pkey");

  Utils::writeFile(import_config.uptane_private_key_path.get(import_config.base_path).string(),
                   std::string("uptane_private_1"));
  Utils::writeFile(import_config.uptane_public_key_path.get(import_config.base_path).string(),
                   std::string("uptane_public_1"));
  Utils::writeFile(import_config.tls_cacert_path.get(import_config.base_path).string(), std::string("tls_cacert_1"));
  Utils::writeFile(import_config.tls_clientcert_path.get(import_config.base_path).string(), std::string("tls_cert_1"));
  Utils::writeFile(import_config.tls_pkey_path.get(import_config.base_path).string(), std::string("tls_pkey_1"));

  // Initially the storage is empty
  EXPECT_FALSE(storage->loadPrimaryPublic(nullptr));
  EXPECT_FALSE(storage->loadPrimaryPrivate(nullptr));
  EXPECT_FALSE(storage->loadTlsCa(nullptr));
  EXPECT_FALSE(storage->loadTlsCert(nullptr));
  EXPECT_FALSE(storage->loadTlsPkey(nullptr));

  storage->importData(import_config);

  std::string primary_public;
  std::string primary_private;
  std::string tls_ca;
  std::string tls_cert;
  std::string tls_pkey;

  // the data has been imported
  EXPECT_TRUE(storage->loadPrimaryPublic(&primary_public));
  EXPECT_TRUE(storage->loadPrimaryPrivate(&primary_private));
  EXPECT_TRUE(storage->loadTlsCa(&tls_ca));
  EXPECT_TRUE(storage->loadTlsCert(&tls_cert));
  EXPECT_TRUE(storage->loadTlsPkey(&tls_pkey));

  EXPECT_EQ(primary_private, "uptane_private_1");
  EXPECT_EQ(primary_public, "uptane_public_1");
  EXPECT_EQ(tls_ca, "tls_cacert_1");
  EXPECT_EQ(tls_cert, "tls_cert_1");
  EXPECT_EQ(tls_pkey, "tls_pkey_1");

  Utils::writeFile(import_config.uptane_private_key_path.get(import_config.base_path).string(),
                   std::string("uptane_private_2"));
  Utils::writeFile(import_config.uptane_public_key_path.get(import_config.base_path).string(),
                   std::string("uptane_public_2"));
  Utils::writeFile(import_config.tls_cacert_path.get(import_config.base_path).string(), std::string("tls_cacert_2"));
  Utils::writeFile(import_config.tls_clientcert_path.get(import_config.base_path).string(), std::string("tls_cert_2"));
  Utils::writeFile(import_config.tls_pkey_path.get(import_config.base_path).string(), std::string("tls_pkey_2"));

  storage->importData(import_config);

  EXPECT_TRUE(storage->loadPrimaryPublic(&primary_public));
  EXPECT_TRUE(storage->loadPrimaryPrivate(&primary_private));
  EXPECT_TRUE(storage->loadTlsCa(&tls_ca));
  EXPECT_TRUE(storage->loadTlsCert(&tls_cert));
  EXPECT_TRUE(storage->loadTlsPkey(&tls_pkey));

  // only root cert is being updated
  EXPECT_EQ(primary_private, "uptane_private_1");
  EXPECT_EQ(primary_public, "uptane_public_1");
  EXPECT_EQ(tls_ca, "tls_cacert_2");
  EXPECT_EQ(tls_cert, "tls_cert_1");
  EXPECT_EQ(tls_pkey, "tls_pkey_1");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  std::cout << "Running tests for SQLStorage" << std::endl;
  current_storage_type = StorageType::kSqlite;
  int res_sql = RUN_ALL_TESTS();

  return res_sql;  // 0 indicates success
}
#endif
