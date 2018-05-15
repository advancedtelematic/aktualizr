#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <boost/filesystem.hpp>

#include "logging/logging.h"
#include "storage/fsstorage.h"
#include "storage/sqlstorage.h"
#include "utilities/utils.h"

boost::filesystem::path storage_test_dir;
StorageConfig storage_test_config;

std::unique_ptr<INvStorage> Storage(const StorageConfig &config) {
  if (config.type == kFileSystem) {
    return std::unique_ptr<INvStorage>(new FSStorage(config));
  } else if (config.type == kSqlite) {
    return std::unique_ptr<INvStorage>(new SQLStorage(config));
  } else {
    std::cout << "Invalid config type: " << config.type << "\n";
    return std::unique_ptr<INvStorage>(nullptr);
  }
}

std::unique_ptr<INvStorage> Storage() { return Storage(storage_test_config); }

StorageConfig MakeConfig(StorageType type, boost::filesystem::path storage_dir) {
  StorageConfig config;

  config.type = type;
  if (type == kFileSystem) {
    config.path = storage_dir;
    config.uptane_metadata_path = "metadata";
    config.uptane_public_key_path = "test_primary.pub";
    config.uptane_private_key_path = "test_primary.priv";
    config.tls_pkey_path = "test_tls.pkey";
    config.tls_clientcert_path = "test_tls.cert";
    config.tls_cacert_path = "test_tls.ca";
  } else if (config.type == kSqlite) {
    config.sqldb_path = storage_dir / "test.db";
    config.schemas_path = "config/schemas";
  } else {
    std::cout << "Invalid config type: " << config.type << "\n";
  }
  return config;
}

TEST(storage, load_store_primary_keys) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  storage->storePrimaryKeys("pr_public", "pr_private");

  std::string pubkey;
  std::string privkey;

  EXPECT_TRUE(storage->loadPrimaryKeys(&pubkey, &privkey));
  EXPECT_EQ(pubkey, "pr_public");
  EXPECT_EQ(privkey, "pr_private");
  storage->clearPrimaryKeys();
  EXPECT_FALSE(storage->loadPrimaryKeys(NULL, NULL));
  boost::filesystem::remove_all(storage_test_dir);
}

TEST(storage, load_store_tls) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  storage->storeTlsCreds("ca", "cert", "priv");
  std::string ca;
  std::string cert;
  std::string priv;

  EXPECT_TRUE(storage->loadTlsCreds(&ca, &cert, &priv));

  EXPECT_EQ(ca, "ca");
  EXPECT_EQ(cert, "cert");
  EXPECT_EQ(priv, "priv");
  storage->clearTlsCreds();
  EXPECT_FALSE(storage->loadTlsCreds(NULL, NULL, NULL));

  boost::filesystem::remove_all(storage_test_dir);
}

TEST(storage, load_store_metadata) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  Uptane::MetaPack stored_meta;

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
  stored_meta.director_root = Uptane::Root("director", meta_root);
  stored_meta.image_root = Uptane::Root("repo", meta_root);

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
  stored_meta.director_targets = Uptane::Targets(meta_targets);
  stored_meta.image_targets = Uptane::Targets(meta_targets);

  Json::Value timestamp_json;
  timestamp_json["signed"]["_type"] = "Timestamp";
  timestamp_json["signed"]["expires"] = "2038-01-19T03:14:06Z";
  stored_meta.image_timestamp = Uptane::TimestampMeta(timestamp_json);

  Json::Value snapshot_json;
  snapshot_json["_type"] = "Snapshot";
  snapshot_json["expires"] = "2038-01-19T03:14:06Z";
  snapshot_json["meta"]["root.json"]["version"] = 1;
  snapshot_json["meta"]["targets.json"]["version"] = 2;
  snapshot_json["meta"]["timestamp.json"]["version"] = 3;
  snapshot_json["meta"]["snapshot.json"]["version"] = 4;

  Json::Value meta_snapshot;
  meta_snapshot["signed"] = snapshot_json;
  stored_meta.image_snapshot = Uptane::Snapshot(meta_snapshot);

  Uptane::MetaPack loaded_meta;

  storage->storeMetadata(stored_meta);
  bool res = storage->loadMetadata(&loaded_meta);

  EXPECT_TRUE(res);
  EXPECT_EQ(stored_meta.director_root, loaded_meta.director_root);
  EXPECT_EQ(stored_meta.director_targets, loaded_meta.director_targets);
  EXPECT_EQ(stored_meta.image_root, loaded_meta.image_root);
  EXPECT_EQ(stored_meta.image_targets, loaded_meta.image_targets);
  EXPECT_EQ(stored_meta.image_timestamp, loaded_meta.image_timestamp);
  EXPECT_EQ(stored_meta.image_snapshot, loaded_meta.image_snapshot);

  boost::filesystem::remove_all(storage_test_dir);
}

TEST(storage, load_store_deviceid) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  storage->storeDeviceId("device_id");

  std::string device_id;

  EXPECT_TRUE(storage->loadDeviceId(&device_id));

  EXPECT_EQ(device_id, "device_id");
  storage->clearDeviceId();
  EXPECT_FALSE(storage->loadDeviceId(NULL));
  boost::filesystem::remove_all(storage_test_dir);
}

TEST(storage, load_store_ecu_serials) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  std::vector<std::pair<std::string, std::string> > serials;
  serials.push_back(std::pair<std::string, std::string>("primary", "primary_hw"));
  serials.push_back(std::pair<std::string, std::string>("secondary_1", "secondary_hw"));
  serials.push_back(std::pair<std::string, std::string>("secondary_2", "secondary_hw"));
  storage->storeEcuSerials(serials);

  std::vector<std::pair<std::string, std::string> > serials_out;

  EXPECT_TRUE(storage->loadEcuSerials(&serials_out));

  EXPECT_EQ(serials, serials_out);
  storage->clearEcuSerials();
  EXPECT_FALSE(storage->loadEcuSerials(NULL));
  boost::filesystem::remove_all(storage_test_dir);
}

TEST(storage, load_store_misconfigured_ecus) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  std::vector<MisconfiguredEcu> ecus;
  ecus.push_back(MisconfiguredEcu("primary", "primary_hw", kNotRegistered));

  storage->storeMisconfiguredEcus(ecus);

  std::vector<MisconfiguredEcu> ecus_out;

  EXPECT_TRUE(storage->loadMisconfiguredEcus(&ecus_out));

  EXPECT_EQ(ecus_out.size(), ecus.size());
  EXPECT_EQ(ecus_out[0].serial, std::string("primary"));
  EXPECT_EQ(ecus_out[0].hardware_id, std::string("primary_hw"));
  EXPECT_EQ(ecus_out[0].state, kNotRegistered);

  storage->clearMisconfiguredEcus();
  ecus_out.clear();
  EXPECT_FALSE(storage->loadMisconfiguredEcus(&ecus_out));
  boost::filesystem::remove_all(storage_test_dir);
}

TEST(storage, load_store_ecu_registered) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();
  storage->storeEcuRegistered();

  EXPECT_TRUE(storage->loadEcuRegistered());

  storage->clearEcuRegistered();
  EXPECT_FALSE(storage->loadEcuRegistered());
  boost::filesystem::remove_all(storage_test_dir);
}

TEST(storage, store_target) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  std::unique_ptr<INvStorage> storage = Storage();

  // write
  {
    std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, "testfile", 2);
    const uint8_t wb[] = "ab";
    fhandle->wfeed(wb, 1);
    fhandle->wfeed(wb + 1, 1);
    fhandle->wcommit();
  }

  // read
  {
    std::unique_ptr<StorageTargetRHandle> rhandle = storage->openTargetFile("testfile");
    uint8_t rb[3] = {0};
    EXPECT_EQ(rhandle->rsize(), 2);
    rhandle->rread(rb, 1);
    rhandle->rread(rb + 1, 1);
    rhandle->rclose();
    EXPECT_STREQ(reinterpret_cast<char *>(rb), "ab");
  }

  // write again
  {
    std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, "testfile", 2);
    const uint8_t wb[] = "ab";
    fhandle->wfeed(wb, 1);
    fhandle->wfeed(wb + 1, 1);
    fhandle->wcommit();
  }

  // delete
  {
    storage->removeTargetFile("testfile");
    EXPECT_THROW(storage->openTargetFile("testfile"), StorageTargetRHandle::ReadError);
    EXPECT_THROW(storage->removeTargetFile("testfile"), std::runtime_error);
  }

  // write stream
  {
    std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, "testfile", 2);

    std::stringstream("ab") >> *fhandle;
  }

  // read stream
  {
    std::stringstream sstr;
    std::unique_ptr<StorageTargetRHandle> rhandle = storage->openTargetFile("testfile");
    sstr << *rhandle;
    EXPECT_STREQ(sstr.str().c_str(), "ab");
  }

  boost::filesystem::remove_all(storage_test_dir);
}

TEST(storage, import_data) {
  mkdir(storage_test_dir.c_str(), S_IRWXU);
  boost::filesystem::create_directories(storage_test_dir / "import");

  std::unique_ptr<INvStorage> storage = Storage();

  ImportConfig import_config;
  import_config.uptane_private_key_path = storage_test_dir / "import" / "private";
  import_config.uptane_public_key_path = storage_test_dir / "import" / "public";
  import_config.tls_cacert_path = storage_test_dir / "import" / "ca";
  import_config.tls_clientcert_path = storage_test_dir / "import" / "cert";
  import_config.tls_pkey_path = storage_test_dir / "import" / "pkey";

  Utils::writeFile(import_config.uptane_private_key_path.string(), std::string("uptane_private_1"));
  Utils::writeFile(import_config.uptane_public_key_path.string(), std::string("uptane_public_1"));
  Utils::writeFile(import_config.tls_cacert_path.string(), std::string("tls_cacert_1"));
  Utils::writeFile(import_config.tls_clientcert_path.string(), std::string("tls_cert_1"));
  Utils::writeFile(import_config.tls_pkey_path.string(), std::string("tls_pkey_1"));

  // Initially the storage is empty
  EXPECT_FALSE(storage->loadPrimaryPublic(NULL));
  EXPECT_FALSE(storage->loadPrimaryPrivate(NULL));
  EXPECT_FALSE(storage->loadTlsCa(NULL));
  EXPECT_FALSE(storage->loadTlsCert(NULL));
  EXPECT_FALSE(storage->loadTlsPkey(NULL));

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

  Utils::writeFile(import_config.uptane_private_key_path.string(), std::string("uptane_private_2"));
  Utils::writeFile(import_config.uptane_public_key_path.string(), std::string("uptane_public_2"));
  Utils::writeFile(import_config.tls_cacert_path.string(), std::string("tls_cacert_2"));
  Utils::writeFile(import_config.tls_clientcert_path.string(), std::string("tls_cert_2"));
  Utils::writeFile(import_config.tls_pkey_path.string(), std::string("tls_pkey_2"));

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
  std::cout << "Running tests for FSStorage" << std::endl;
  TemporaryDirectory temp_dir1;
  storage_test_dir = temp_dir1.Path();
  storage_test_config = MakeConfig(kFileSystem, storage_test_dir);
  int res_fs = RUN_ALL_TESTS();

  std::cout << "Running tests for SQLStorage" << std::endl;
  TemporaryDirectory temp_dir2;
  storage_test_dir = temp_dir2.Path();
  storage_test_config = MakeConfig(kSqlite, storage_test_dir);
  int res_sql = RUN_ALL_TESTS();

  return res_fs || res_sql;  // 0 indicates success
}
#endif
