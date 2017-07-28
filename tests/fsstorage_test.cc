#include <gtest/gtest.h>
#include <string>

#include "fsstorage.h"

#include "logger.h"

TEST(fsstorage, load_store_ecu) {
  Config config;
  config.tls.certificates_directory = "tests/test_data/test_fsstorage";
  config.uptane.metadata_path = "tests/test_data/test_fsstorage";
  config.uptane.public_key_path = "test_primary.pub";
  config.uptane.private_key_path = "test_primary.priv";

  FSStorage storage(config);
  storage.storeEcu(true, "test_primary_hw", "test_primary_serial", "pr_public", "pr_private");
  storage.storeEcu(false, "test_secondary_hw", "test_secondary1_serial", "sec1_public", "sec1_private");
  storage.storeEcu(false, "test_secondary_hw", "test_secondary2_serial", "sec2_public", "sec2_private");

  std::string pubkey;
  std::string privkey;

  storage.loadEcuKeys(true, "test_primary_hw", "test_primary_serial", &pubkey, &privkey);
  EXPECT_EQ(pubkey, "pr_public");
  EXPECT_EQ(privkey, "pr_private");

  storage.loadEcuKeys(true, "test_secondary_hw", "test_secondary1_serial", &pubkey, &privkey);
  EXPECT_EQ(pubkey, "pr_public");
  EXPECT_EQ(privkey, "pr_private");
  
  storage.loadEcuKeys(true, "test_secondary_hw", "test_secondary2_serial", &pubkey, &privkey);
  EXPECT_EQ(pubkey, "pr_public");
  EXPECT_EQ(privkey, "pr_private");
}

TEST(fsstorage, load_store_bootstrap_tls) {
  Config config;
  config.tls.certificates_directory = "tests/test_data/test_fsstorage";
  config.uptane.metadata_path = "tests/test_data/test_fsstorage";

  FSStorage storage(config);
  storage.storeBootstrapTlsCreds("ca", "cert", "priv");

  std::string ca;
  std::string cert;
  std::string priv;

  storage.loadBootstrapTlsCreds(&ca, &cert, &priv);

  EXPECT_EQ(ca, "ca");
  EXPECT_EQ(cert, "cert");
  EXPECT_EQ(priv, "priv");
}

TEST(fsstorage, load_store_tls) {
  Config config;
  config.tls.certificates_directory = "tests/test_data/test_fsstorage";
  config.uptane.metadata_path = "tests/test_data/test_fsstorage";
  config.tls.pkey_file = "test_tls.pkey";
  config.tls.client_certificate = "test_tls.cert";
  config.tls.ca_file = "test_tls.ca";

  FSStorage storage(config);
  storage.storeBootstrapTlsCreds("ca", "cert", "priv");

  std::string ca;
  std::string cert;
  std::string priv;

  storage.loadBootstrapTlsCreds(&ca, &cert, &priv);

  EXPECT_EQ(ca, "ca");
  EXPECT_EQ(cert, "cert");
  EXPECT_EQ(priv, "priv");
}

TEST(fsstorage, loadstoremetadata) {
  Config config;
  config.tls.certificates_directory = "tests/test_data/test_fsstorage";
  config.uptane.metadata_path = "tests/test_data/test_fsstorage/metadata";

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
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerSetSeverity(LVL_trace);
  return RUN_ALL_TESTS();
}
#endif

