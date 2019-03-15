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

/* Load and store primary keys. */
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

  storage->storeRoot(director_root, Uptane::RepositoryType::Director(), Uptane::Version(1));
  storage->storeNonRoot(director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
  storage->storeRoot(images_root, Uptane::RepositoryType::Image(), Uptane::Version(1));
  storage->storeNonRoot(images_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets());
  storage->storeNonRoot(images_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());
  storage->storeNonRoot(images_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());

  std::string loaded_director_root;
  std::string loaded_director_targets;
  std::string loaded_images_root;
  std::string loaded_images_targets;
  std::string loaded_images_timestamp;
  std::string loaded_images_snapshot;

  EXPECT_TRUE(storage->loadLatestRoot(&loaded_director_root, Uptane::RepositoryType::Director()));
  EXPECT_TRUE(
      storage->loadNonRoot(&loaded_director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));
  EXPECT_TRUE(storage->loadLatestRoot(&loaded_images_root, Uptane::RepositoryType::Image()));
  EXPECT_TRUE(storage->loadNonRoot(&loaded_images_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets()));
  EXPECT_TRUE(
      storage->loadNonRoot(&loaded_images_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));
  EXPECT_TRUE(storage->loadNonRoot(&loaded_images_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()));
  EXPECT_EQ(director_root, loaded_director_root);
  EXPECT_EQ(director_targets, loaded_director_targets);
  EXPECT_EQ(images_root, loaded_images_root);
  EXPECT_EQ(images_targets, loaded_images_targets);
  EXPECT_EQ(images_timestamp, loaded_images_timestamp);
  EXPECT_EQ(images_snapshot, loaded_images_snapshot);

  storage->clearNonRootMeta(Uptane::RepositoryType::Director());
  storage->clearNonRootMeta(Uptane::RepositoryType::Image());
  EXPECT_FALSE(
      storage->loadNonRoot(&loaded_director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));
  EXPECT_FALSE(
      storage->loadNonRoot(&loaded_images_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));
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

/* Load and store ECU serials. */
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

  // Test lazy primary installed version: primary ecu serial is not defined yet
  const std::vector<Uptane::Hash> hashes = {
      Uptane::Hash{Uptane::Hash::Type::kSha256, "2561"},
      Uptane::Hash{Uptane::Hash::Type::kSha512, "5121"},
  };
  Uptane::Target t1{"update.bin", hashes, 1, "corrid"};
  storage->savePrimaryInstalledVersion(t1, InstalledVersionUpdateMode::kCurrent);

  EcuSerials serials{{Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw")},
                     {Uptane::EcuSerial("secondary_1"), Uptane::HardwareIdentifier("secondary_hw")},
                     {Uptane::EcuSerial("secondary_2"), Uptane::HardwareIdentifier("secondary_hw")}};
  storage->storeEcuSerials(serials);

  {
    std::vector<Uptane::Target> installed_versions;
    size_t current = SIZE_MAX;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", &installed_versions, &current, nullptr));
    EXPECT_EQ(installed_versions.size(), 1);
    EXPECT_EQ(installed_versions.at(current).filename(), "update.bin");
    EXPECT_EQ(installed_versions.at(current).sha256Hash(), "2561");
    EXPECT_EQ(installed_versions.at(current).hashes(), hashes);
    EXPECT_EQ(installed_versions.at(current).correlation_id(), "corrid");
    EXPECT_EQ(installed_versions.at(current).length(), 1);
  }

  // Set t2 as a pending version
  Uptane::Target t2{"update2.bin", {Uptane::Hash{Uptane::Hash::Type::kSha256, "2562"}}, 2};
  storage->savePrimaryInstalledVersion(t2, InstalledVersionUpdateMode::kPending);

  {
    std::vector<Uptane::Target> installed_versions;
    size_t pending = SIZE_MAX;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", &installed_versions, nullptr, &pending));
    EXPECT_EQ(installed_versions.size(), 2);
    EXPECT_EQ(installed_versions.at(pending).filename(), "update2.bin");
  }

  // Set t3 as the new pending
  Uptane::Target t3{"update3.bin", {Uptane::Hash{Uptane::Hash::Type::kSha256, "2563"}}, 3};
  storage->savePrimaryInstalledVersion(t3, InstalledVersionUpdateMode::kPending);

  {
    std::vector<Uptane::Target> installed_versions;
    size_t pending = SIZE_MAX;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", &installed_versions, nullptr, &pending));
    EXPECT_EQ(installed_versions.size(), 3);
    EXPECT_EQ(installed_versions.at(pending).filename(), "update3.bin");
  }

  // Set t3 as current: should replace the pending flag but not create a new
  // version
  storage->savePrimaryInstalledVersion(t3, InstalledVersionUpdateMode::kCurrent);

  {
    std::vector<Uptane::Target> installed_versions;
    size_t current = SIZE_MAX;
    size_t pending = SIZE_MAX;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", &installed_versions, &current, &pending));
    EXPECT_EQ(installed_versions.size(), 3);
    EXPECT_EQ(installed_versions.at(current).filename(), "update3.bin");
    EXPECT_EQ(pending, SIZE_MAX);
  }

  // Set t2 as the new pending and t3 as current afterwards: the pending flag
  // should disappear
  storage->savePrimaryInstalledVersion(t2, InstalledVersionUpdateMode::kPending);
  storage->savePrimaryInstalledVersion(t3, InstalledVersionUpdateMode::kCurrent);

  {
    std::vector<Uptane::Target> installed_versions;
    size_t current = SIZE_MAX;
    size_t pending = SIZE_MAX;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", &installed_versions, &current, &pending));
    EXPECT_EQ(installed_versions.size(), 3);
    EXPECT_EQ(installed_versions.at(current).filename(), "update3.bin");
    EXPECT_EQ(pending, SIZE_MAX);
  }

  // Add a secondary installed version
  Uptane::Target tsec{"secondary.bin", {Uptane::Hash{Uptane::Hash::Type::kSha256, "256s"}}, 4};
  storage->saveInstalledVersion("secondary_1", tsec, InstalledVersionUpdateMode::kCurrent);

  {
    std::vector<Uptane::Target> installed_versions;
    EXPECT_TRUE(storage->loadInstalledVersions("primary", &installed_versions, nullptr, nullptr));
    EXPECT_EQ(installed_versions.size(), 3);

    EXPECT_TRUE(storage->loadInstalledVersions("secondary_1", &installed_versions, nullptr, nullptr));
    EXPECT_EQ(installed_versions.size(), 1);
  }
}

/*
 * Load and store an ecu installation result in an SQL database.
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

  EXPECT_TRUE(storage->loadEcuInstallationResults(&res));
  EXPECT_EQ(res.size(), 0);
  EXPECT_FALSE(storage->loadDeviceInstallationResult(&dev_res, &report, &correlation_id));
}

/* Load and store targets. */
TEST(storage, store_target) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

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
}

TEST(storage, checksum) {
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

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
  TemporaryDirectory temp_dir;
  std::unique_ptr<INvStorage> storage = Storage(temp_dir.Path());

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash1";
  target_json["length"] = 3;
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

  // Append without committing, should commit in whandle destructor
  {
    std::unique_ptr<StorageTargetWHandle> whandle = storage->openTargetFile(target)->toWriteHandle();
    const uint8_t wb[] = "b";
    whandle->wfeed(wb, 1);
  }

  // read and check partial target
  {
    std::unique_ptr<StorageTargetRHandle> rhandle = storage->openTargetFile(target);
    uint8_t rb[3] = {0};
    EXPECT_EQ(rhandle->rsize(), 2);
    EXPECT_TRUE(rhandle->isPartial());
    rhandle->rread(rb, 2);
    rhandle->rclose();
    EXPECT_STREQ(reinterpret_cast<char *>(rb), "ab");
  }

  // Append partial
  {
    std::unique_ptr<StorageTargetWHandle> whandle = storage->openTargetFile(target)->toWriteHandle();
    const uint8_t wb[] = "c";
    whandle->wfeed(wb, 1);
    whandle->wcommit();
  }

  // Check full target
  {
    std::unique_ptr<StorageTargetRHandle> rhandle = storage->openTargetFile(target);
    EXPECT_EQ(rhandle->rsize(), 3);
    EXPECT_FALSE(rhandle->isPartial());
  }
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
