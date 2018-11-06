#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <boost/filesystem.hpp>

#include "logging/logging.h"
#include "storage/sqlstorage.h"
#include "utilities/types.h"
#include "utilities/utils.h"

boost::filesystem::path storage_test_dir;
StorageConfig storage_test_config;

std::unique_ptr<INvStorage> Storage(const StorageConfig &config) {
  if (config.type == StorageType::kSqlite) {
    return std::unique_ptr<INvStorage>(new SQLStorage(config, false));
  } else {
    throw std::runtime_error("Invalid config type");
  }
}

std::unique_ptr<INvStorage> Storage() { return Storage(storage_test_config); }

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

/* Load and store primary keys. */
TEST(storage, load_store_primary_keys) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  storage->storePrimaryKeys("", "");
  storage->storePrimaryKeys("pr_public", "pr_private");

  std::string pubkey;
  std::string privkey;

  EXPECT_TRUE(storage->loadPrimaryKeys(&pubkey, &privkey));
  EXPECT_EQ(pubkey, "pr_public");
  EXPECT_EQ(privkey, "pr_private");
  storage->clearPrimaryKeys();
  EXPECT_FALSE(storage->loadPrimaryKeys(nullptr, nullptr));
  boost::filesystem::remove_all(storage_test_dir);
}

/* Load and store TLS credentials. */
TEST(storage, load_store_tls) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
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

  boost::filesystem::remove_all(storage_test_dir);
}

/* Load and store Uptane metadata. */
TEST(storage, load_store_metadata) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();

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
  std::string images_root = Utils::jsonToStr(meta_root);

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
  std::string images_targets = Utils::jsonToStr(meta_targets);

  Json::Value timestamp_json;
  timestamp_json["signed"]["_type"] = "Timestamp";
  timestamp_json["signed"]["expires"] = "2038-01-19T03:14:06Z";
  std::string images_timestamp = Utils::jsonToStr(timestamp_json);

  Json::Value snapshot_json;
  snapshot_json["_type"] = "Snapshot";
  snapshot_json["expires"] = "2038-01-19T03:14:06Z";
  snapshot_json["meta"]["root.json"]["version"] = 1;
  snapshot_json["meta"]["targets.json"]["version"] = 2;
  snapshot_json["meta"]["timestamp.json"]["version"] = 3;
  snapshot_json["meta"]["snapshot.json"]["version"] = 4;

  Json::Value meta_snapshot;
  meta_snapshot["signed"] = snapshot_json;
  std::string images_snapshot = Utils::jsonToStr(meta_snapshot);

  storage->storeRoot(director_root, Uptane::RepositoryType::Director, Uptane::Version(1));
  storage->storeNonRoot(director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets());
  storage->storeRoot(images_root, Uptane::RepositoryType::Images, Uptane::Version(1));
  storage->storeNonRoot(images_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets());
  storage->storeNonRoot(images_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp());
  storage->storeNonRoot(images_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot());

  std::string loaded_director_root;
  std::string loaded_director_targets;
  std::string loaded_images_root;
  std::string loaded_images_targets;
  std::string loaded_images_timestamp;
  std::string loaded_images_snapshot;

  EXPECT_TRUE(storage->loadLatestRoot(&loaded_director_root, Uptane::RepositoryType::Director));
  EXPECT_TRUE(
      storage->loadNonRoot(&loaded_director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets()));
  EXPECT_TRUE(storage->loadLatestRoot(&loaded_images_root, Uptane::RepositoryType::Images));
  EXPECT_TRUE(storage->loadNonRoot(&loaded_images_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets()));
  EXPECT_TRUE(
      storage->loadNonRoot(&loaded_images_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp()));
  EXPECT_TRUE(storage->loadNonRoot(&loaded_images_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot()));
  EXPECT_EQ(director_root, loaded_director_root);
  EXPECT_EQ(director_targets, loaded_director_targets);
  EXPECT_EQ(images_root, loaded_images_root);
  EXPECT_EQ(images_targets, loaded_images_targets);
  EXPECT_EQ(images_timestamp, loaded_images_timestamp);
  EXPECT_EQ(images_snapshot, loaded_images_snapshot);

  storage->clearNonRootMeta(Uptane::RepositoryType::Director);
  storage->clearNonRootMeta(Uptane::RepositoryType::Images);
  EXPECT_FALSE(
      storage->loadNonRoot(&loaded_director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets()));
  EXPECT_FALSE(
      storage->loadNonRoot(&loaded_images_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp()));

  boost::filesystem::remove_all(storage_test_dir);
}

/* Load and store Uptane roots. */
TEST(storage, load_store_root) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();

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

  storage->storeRoot(Utils::jsonToStr(meta_root), Uptane::RepositoryType::Director, Uptane::Version(2));
  EXPECT_TRUE(storage->loadRoot(&loaded_root, Uptane::RepositoryType::Director, Uptane::Version(2)));
  EXPECT_EQ(Utils::jsonToStr(meta_root), loaded_root);

  EXPECT_TRUE(storage->loadLatestRoot(&loaded_root, Uptane::RepositoryType::Director));
  EXPECT_EQ(Utils::jsonToStr(meta_root), loaded_root);

  boost::filesystem::remove_all(storage_test_dir);
}

/* Load and store the device ID. */
TEST(storage, load_store_deviceid) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  storage->storeDeviceId("");
  storage->storeDeviceId("device_id");

  std::string device_id;

  EXPECT_TRUE(storage->loadDeviceId(&device_id));

  EXPECT_EQ(device_id, "device_id");
  storage->clearDeviceId();
  EXPECT_FALSE(storage->loadDeviceId(nullptr));
  boost::filesystem::remove_all(storage_test_dir);
}

/* Load and store ECU serials. */
TEST(storage, load_store_ecu_serials) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
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
  boost::filesystem::remove_all(storage_test_dir);
}

/* Load and store a list of misconfigured ECUs. */
TEST(storage, load_store_misconfigured_ecus) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  std::vector<MisconfiguredEcu> ecus;
  ecus.push_back(MisconfiguredEcu(Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw"),
                                  EcuState::kNotRegistered));

  storage->storeMisconfiguredEcus(ecus);

  std::vector<MisconfiguredEcu> ecus_out;

  EXPECT_TRUE(storage->loadMisconfiguredEcus(&ecus_out));

  EXPECT_EQ(ecus_out.size(), ecus.size());
  EXPECT_EQ(ecus_out[0].serial, Uptane::EcuSerial("primary"));
  EXPECT_EQ(ecus_out[0].hardware_id, Uptane::HardwareIdentifier("primary_hw"));
  EXPECT_EQ(ecus_out[0].state, EcuState::kNotRegistered);

  storage->clearMisconfiguredEcus();
  ecus_out.clear();
  EXPECT_FALSE(storage->loadMisconfiguredEcus(&ecus_out));
  boost::filesystem::remove_all(storage_test_dir);
}

/* Load and store a flag indicating successful registration. */
TEST(storage, load_store_ecu_registered) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  EXPECT_THROW(storage->storeEcuRegistered(), std::runtime_error);
  storage->storeDeviceId("test");
  storage->storeEcuRegistered();
  storage->storeEcuRegistered();

  EXPECT_TRUE(storage->loadEcuRegistered());

  storage->clearEcuRegistered();
  EXPECT_FALSE(storage->loadEcuRegistered());
  boost::filesystem::remove_all(storage_test_dir);
}

/* Load and store an installation result. */
TEST(storage, load_store_installation_result) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();

  storage->storeInstallationResult(
      data::OperationResult("", data::InstallOutcome(data::UpdateResultCode::kGeneralError, "")));
  data::InstallOutcome outcome(data::UpdateResultCode::kGeneralError, "some failure");
  data::OperationResult result("target_filename", outcome);
  storage->storeInstallationResult(result);

  data::OperationResult result_out;
  EXPECT_TRUE(storage->loadInstallationResult(&result_out));
  EXPECT_EQ(result.id, result_out.id);
  EXPECT_EQ(result.result_code, result_out.result_code);
  EXPECT_EQ(result.result_text, result_out.result_text);
  storage->clearInstallationResult();
  EXPECT_FALSE(storage->loadInstallationResult(nullptr));
  boost::filesystem::remove_all(storage_test_dir);
}

/* Load and store targets. */
TEST(storage, store_target) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash";
  target_json["length"] = 2;
  Uptane::Target target("some.deb", target_json);

  // write
  {
    std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, target);
    const uint8_t wb[] = "ab";
    fhandle->wfeed(wb, 1);
    fhandle->wfeed(wb + 1, 1);
    fhandle->wcommit();
  }

  // read
  {
    std::unique_ptr<StorageTargetRHandle> rhandle = storage->openTargetFile(target);
    uint8_t rb[3] = {0};
    EXPECT_EQ(rhandle->rsize(), 2);
    rhandle->rread(rb, 1);
    rhandle->rread(rb + 1, 1);
    rhandle->rclose();
    EXPECT_STREQ(reinterpret_cast<char *>(rb), "ab");
  }

  // write again
  {
    std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, target);
    const uint8_t wb[] = "ab";
    fhandle->wfeed(wb, 1);
    fhandle->wfeed(wb + 1, 1);
    fhandle->wcommit();
  }

  // delete
  {
    storage->removeTargetFile(target.filename());
    EXPECT_THROW(storage->openTargetFile(target), StorageTargetRHandle::ReadError);
    EXPECT_THROW(storage->removeTargetFile(target.filename()), std::runtime_error);
  }

  // write stream
  {
    std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, target);

    std::stringstream("ab") >> *fhandle;
  }

  // read stream
  {
    std::stringstream sstr;
    std::unique_ptr<StorageTargetRHandle> rhandle = storage->openTargetFile(target);
    sstr << *rhandle;
    EXPECT_STREQ(sstr.str().c_str(), "ab");
  }

  boost::filesystem::remove_all(storage_test_dir);
}

TEST(storage, checksum) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::cout << "DIR: " << storage_test_dir.c_str() << "\n";
  std::unique_ptr<INvStorage> storage = Storage();
  Json::Value target_json1;
  target_json1["hashes"]["sha256"] = "hash1";
  target_json1["length"] = 2;
  Uptane::Target target1("some.deb", target_json1);
  Json::Value target_json2;
  target_json2["length"] = 2;
  target_json2["hashes"]["sha256"] = "hash2";
  Uptane::Target target2("some.deb", target_json2);

  // write target1
  {
    std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, target1);
    const uint8_t wb[] = "ab";
    fhandle->wfeed(wb, 2);
    fhandle->wcommit();
  }

  // read target 1
  {
    std::unique_ptr<StorageTargetRHandle> rhandle = storage->openTargetFile(target1);
    uint8_t rb[3] = {0};
    EXPECT_EQ(rhandle->rsize(), 2);
    rhandle->rread(rb, 2);
    rhandle->rclose();
    EXPECT_STREQ(reinterpret_cast<char *>(rb), "ab");
  }

  // read target 2
  { EXPECT_THROW(storage->openTargetFile(target2), StorageTargetRHandle::ReadError); }
}

TEST(storage, partial) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash1";
  target_json["length"] = 2;
  Uptane::Target target("some.deb", target_json);

  // write partial target
  {
    std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, target);
    const uint8_t wb[] = "a";
    fhandle->wfeed(wb, 1);
    fhandle->wcommit();
  }

  // read and check partial target

  {
    std::unique_ptr<StorageTargetRHandle> rhandle = storage->openTargetFile(target);
    uint8_t rb[2] = {0};
    EXPECT_EQ(rhandle->rsize(), 1);
    EXPECT_TRUE(rhandle->isPartial());
    rhandle->rread(rb, 1);
    rhandle->rclose();
    EXPECT_STREQ(reinterpret_cast<char *>(rb), "a");
  }

  // Append partial
  {
    std::unique_ptr<StorageTargetWHandle> whandle = storage->openTargetFile(target)->toWriteHandle();
    const uint8_t wb[] = "b";
    whandle->wfeed(wb, 1);
    whandle->wcommit();
  }

  // Check full target
  {
    std::unique_ptr<StorageTargetRHandle> rhandle = storage->openTargetFile(target);
    EXPECT_EQ(rhandle->rsize(), 2);
    EXPECT_FALSE(rhandle->isPartial());
  }
}

/* Import keys and credentials from file into storage. */
TEST(storage, import_data) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  boost::filesystem::create_directories(storage_test_dir / "import");

  std::unique_ptr<INvStorage> storage = Storage();

  ImportConfig import_config;
  import_config.base_path = storage_test_dir / "import";
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

  boost::filesystem::remove_all(storage_test_dir);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  std::cout << "Running tests for SQLStorage" << std::endl;
  TemporaryDirectory temp_dir2;
  storage_test_dir = temp_dir2.Path();
  storage_test_config = MakeConfig(StorageType::kSqlite, storage_test_dir);
  int res_sql = RUN_ALL_TESTS();

  return res_sql;  // 0 indicates success
}
#endif
