#include <gtest/gtest.h>

#include "aktualizr_secondary.h"
#include "uptane/secondaryinterface.h"

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "config/config.h"
#include "storage/fsstorage.h"
#include "utilities/utils.h"
std::string sysroot;

class ShortCircuitSecondary : public Uptane::SecondaryInterface {
 public:
  ShortCircuitSecondary(const Uptane::SecondaryConfig& sconfig_in, AktualizrSecondary& sec)
      : SecondaryInterface(sconfig_in), secondary(sec) {}
  virtual ~ShortCircuitSecondary() {}

  virtual Uptane::EcuSerial getSerial() { return secondary.getSerialResp(); }
  virtual Uptane::HardwareIdentifier getHwId() { return secondary.getHwIdResp(); }
  virtual PublicKey getPublicKey() { return secondary.getPublicKeyResp(); }
  virtual Json::Value getManifest() { return secondary.getManifestResp(); }
  virtual bool putMetadata(const Uptane::RawMetaPack& meta_pack) { return secondary.putMetadataResp(meta_pack); }
  virtual int32_t getRootVersion(bool director) { return secondary.getRootVersionResp(director); }
  virtual bool putRoot(const std::string& root, bool director) { return secondary.putRootResp(root, director); }
  virtual bool sendFirmware(const std::string& data) { return secondary.sendFirmwareResp(data); }

 private:
  AktualizrSecondary& secondary;
};

TEST(aktualizr_secondary_protocol, DISABLED_manual_update) {
  // secondary
  TemporaryDirectory temp_dir_sec;
  AktualizrSecondaryConfig config;
  config.network.port = 0;
  config.storage.type = StorageType::kSqlite;
  config.storage.sqldb_path = temp_dir_sec / "sql.db";
  config.pacman.sysroot = sysroot;
  auto storage = INvStorage::newStorage(config.storage, temp_dir_sec.Path());

  AktualizrSecondary as(config, storage);

  // secondary interface
  Uptane::SecondaryConfig config_iface;
  ShortCircuitSecondary sec_iface{config_iface, as};

  // storage
  TemporaryDirectory temp_dir;
  int ret = system((std::string("cp -rf tests/test_data/secondary_meta/* ") + temp_dir.PathString()).c_str());
  (void)ret;
  StorageConfig fs_config;
  fs_config.type = StorageType::kFileSystem;
  fs_config.path = temp_dir.Path();
  fs_config.uptane_metadata_path = "metadata";
  FSStorage fs_storage(fs_config);

  Uptane::RawMetaPack metadata;
  EXPECT_TRUE(fs_storage.loadRole(&metadata.director_root, Uptane::RepositoryType::Director, Uptane::Role::Root()));
  EXPECT_TRUE(
      fs_storage.loadRole(&metadata.director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets()));
  EXPECT_TRUE(fs_storage.loadRole(&metadata.image_root, Uptane::RepositoryType::Images, Uptane::Role::Root()));
  EXPECT_TRUE(fs_storage.loadRole(&metadata.image_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets()));
  EXPECT_TRUE(
      fs_storage.loadRole(&metadata.image_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp()));
  EXPECT_TRUE(fs_storage.loadRole(&metadata.image_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot()));

  std::string firmware = Utils::readFile(temp_dir.Path() / "firmware.bin");

  EXPECT_TRUE(sec_iface.putMetadata(metadata));
  EXPECT_TRUE(sec_iface.sendFirmware(firmware));
  Json::Value manifest = sec_iface.getManifest();

  EXPECT_EQ(manifest["signed"]["installed_image"]["fileinfo"]["hashes"]["sha256"],
            boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(firmware))));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  sysroot = argv[1];
  return RUN_ALL_TESTS();
}
#endif
