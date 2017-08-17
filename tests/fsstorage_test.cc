#include <gtest/gtest.h>
#include <string>

#include "fsstorage.h"

#include "logger.h"

const std::string fsstorage_test_dir = "tests/test_fsstorage";

TEST(fsstorage, load_store_primary_keys) {
  Config config;
  config.tls.certificates_directory = fsstorage_test_dir;
  config.uptane.metadata_path = fsstorage_test_dir;
  config.uptane.public_key_path = "test_primary.pub";
  config.uptane.private_key_path = "test_primary.priv";

  FSStorage storage(config);
  storage.storePrimaryKeys("pr_public", "pr_private");

  std::string pubkey;
  std::string privkey;

  EXPECT_TRUE(storage.loadPrimaryKeys(&pubkey, &privkey));
  EXPECT_EQ(pubkey, "pr_public");
  EXPECT_EQ(privkey, "pr_private");
  storage.clearPrimaryKeys();
  EXPECT_FALSE(storage.loadPrimaryKeys(NULL, NULL));
  boost::filesystem::remove_all(fsstorage_test_dir);
}

TEST(fsstorage, load_store_tls) {
  Config config;
  config.tls.certificates_directory = fsstorage_test_dir;
  config.uptane.metadata_path = fsstorage_test_dir;
  config.tls.pkey_file = "test_tls.pkey";
  config.tls.client_certificate = "test_tls.cert";
  config.tls.ca_file = "test_tls.ca";

  FSStorage storage(config);
  storage.storeTlsCreds("ca", "cert", "priv");
  std::string ca;
  std::string cert;
  std::string priv;


  EXPECT_TRUE(storage.loadTlsCreds(&ca, &cert, &priv));

  EXPECT_EQ(ca, "ca");
  EXPECT_EQ(cert, "cert");
  EXPECT_EQ(priv, "priv");
  storage.clearTlsCreds();
  EXPECT_FALSE(storage.loadTlsCreds(NULL, NULL, NULL));

  boost::filesystem::remove_all(fsstorage_test_dir);
}

#ifdef BUILD_OSTREE
TEST(fsstorage, load_store_metadata) {
  Config config;
  config.tls.certificates_directory = fsstorage_test_dir;
  config.uptane.metadata_path = fsstorage_test_dir + "/metadata";

  FSStorage storage(config);
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

  stored_meta.director_root = Uptane::Root("director", root_json);
  stored_meta.image_root = Uptane::Root("repo", root_json);

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
  stored_meta.director_targets = Uptane::Targets(targets_json);
  stored_meta.image_targets = Uptane::Targets(targets_json);

  Json::Value timestamp_json;
  timestamp_json["_type"] = "Timestamp";
  timestamp_json["expires"] = "2038-01-19T03:14:06Z";
  stored_meta.image_timestamp = Uptane::TimestampMeta(timestamp_json);

  Json::Value snapshot_json;
  snapshot_json["_type"] = "Snapshot";
  snapshot_json["expires"] = "2038-01-19T03:14:06Z";
  snapshot_json["meta"]["root.json"]["version"] = 1;
  snapshot_json["meta"]["targets.json"]["version"] = 2;
  snapshot_json["meta"]["timestamp.json"]["version"] = 3;
  snapshot_json["meta"]["snapshot.json"]["version"] = 4;
  stored_meta.image_snapshot = Uptane::Snapshot(snapshot_json);

  Uptane::MetaPack loaded_meta;

  storage.storeMetadata(stored_meta);
  bool res = storage.loadMetadata(&loaded_meta);

  EXPECT_TRUE(res);
  EXPECT_EQ(stored_meta.director_root, loaded_meta.director_root);
  EXPECT_EQ(stored_meta.director_targets, loaded_meta.director_targets);
  EXPECT_EQ(stored_meta.image_root, loaded_meta.image_root);
  EXPECT_EQ(stored_meta.image_targets, loaded_meta.image_targets);
  EXPECT_EQ(stored_meta.image_timestamp, loaded_meta.image_timestamp);
  EXPECT_EQ(stored_meta.image_snapshot, loaded_meta.image_snapshot);

  boost::filesystem::remove_all(fsstorage_test_dir);
}
#endif  // BUILD_OSTREE

TEST(fsstorage, load_store_deviceid) {
  Config config;
  config.tls.certificates_directory = fsstorage_test_dir;

  FSStorage storage(config);
  storage.storeDeviceId("device_id");

  std::string device_id;

  EXPECT_TRUE(storage.loadDeviceId(&device_id));

  EXPECT_EQ(device_id, "device_id");
  storage.clearDeviceId();
  EXPECT_FALSE(storage.loadDeviceId(NULL));
  boost::filesystem::remove_all(fsstorage_test_dir);
}

TEST(fsstorage, load_store_ecu_serials) {
  Config config;
  config.tls.certificates_directory = fsstorage_test_dir;

  FSStorage storage(config);
  std::vector<std::pair<std::string, std::string> > serials;
  serials.push_back(std::pair<std::string, std::string>("primary", "primary_hw"));
  serials.push_back(std::pair<std::string, std::string>("secondary_1", "secondary_hw"));
  serials.push_back(std::pair<std::string, std::string>("secondary_2", "secondary_hw"));
  storage.storeEcuSerials(serials);

  std::vector<std::pair<std::string, std::string> > serials_out;

  EXPECT_TRUE(storage.loadEcuSerials(&serials_out));

  EXPECT_EQ(serials, serials_out);
  storage.clearEcuSerials();
  EXPECT_FALSE(storage.loadEcuSerials(NULL));
  boost::filesystem::remove_all(fsstorage_test_dir);
}

TEST(fsstorage, load_store_ecu_registered) {
  Config config;
  config.tls.certificates_directory = fsstorage_test_dir;

  FSStorage storage(config);
  storage.storeEcuRegistered();

  EXPECT_TRUE(storage.loadEcuRegistered());

  storage.clearEcuRegistered();
  EXPECT_FALSE(storage.loadEcuRegistered());
  boost::filesystem::remove_all(fsstorage_test_dir);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerInit();
  loggerSetSeverity(LVL_trace);
  return RUN_ALL_TESTS();
}
#endif

