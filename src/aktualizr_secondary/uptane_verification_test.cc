#include <gtest/gtest.h>

#include "aktualizr_secondary.h"
#include "test_utils.h"

static boost::filesystem::path UPTANE_METADATA_GENERATOR;

class AktualizrSecondaryPtr {
 public:
  AktualizrSecondaryPtr() {
    AktualizrSecondaryConfig config;

    config.pacman.type = PackageManager::kNone;
    config.storage.path = _storage_dir.Path();
    config.storage.type = StorageType::kSqlite;

    auto storage = INvStorage::newStorage(config.storage);
    _secondary = std::make_shared<AktualizrSecondary>(config, storage);
  }

 public:
  std::shared_ptr<AktualizrSecondary>& operator->() { return _secondary; }

 private:
  TemporaryDirectory _storage_dir;
  std::shared_ptr<AktualizrSecondary> _secondary;
};

class UptaneRepo {
 public:
  UptaneRepo() {
    _uptane_gen.run({"generate", "--path", _root_dir.PathString()});
    EXPECT_EQ(_uptane_gen.lastExitCode(), 0) << _uptane_gen.lastStdOut();
  }

  Uptane::RawMetaPack addImageFile(const std::string& targetname, const std::string& hardware_id,
                                   const std::string& serial, const std::string& expires = "") {
    const auto image_file_path = _root_dir / targetname;
    boost::filesystem::ofstream(image_file_path) << "some data";

    _uptane_gen.run({"image", "--path", _root_dir.PathString(), "--targetname", targetname, "--filename",
                     image_file_path.c_str(), "--hwid", hardware_id, "--expires", expires});

    EXPECT_EQ(_uptane_gen.lastExitCode(), 0)
        << _uptane_gen.lastStdOut(); /* uptane_gen output message into stdout in case of an error */

    _uptane_gen.run({"addtarget", "--path", _root_dir.PathString(), "--targetname", targetname, "--hwid", hardware_id,
                     "--serial", serial, "--expires", expires});
    EXPECT_EQ(_uptane_gen.lastExitCode(), 0) << _uptane_gen.lastStdOut();

    _uptane_gen.run({"signtargets", "--path", _root_dir.PathString()});
    EXPECT_EQ(_uptane_gen.lastExitCode(), 0) << _uptane_gen.lastStdOut();

    return getCurrentMetadata();
  }

  Uptane::RawMetaPack getCurrentMetadata() const {
    Uptane::RawMetaPack metadata;

    boost::filesystem::load_string_file(_director_dir / "root.json", metadata.director_root);
    boost::filesystem::load_string_file(_director_dir / "targets.json", metadata.director_targets);

    boost::filesystem::load_string_file(_imagerepo_dir / "root.json", metadata.image_root);
    boost::filesystem::load_string_file(_imagerepo_dir / "timestamp.json", metadata.image_timestamp);
    boost::filesystem::load_string_file(_imagerepo_dir / "snapshot.json", metadata.image_snapshot);
    boost::filesystem::load_string_file(_imagerepo_dir / "targets.json", metadata.image_targets);

    return metadata;
  }

  std::shared_ptr<std::string> getImageData(const std::string& targetname) const {
    auto image_data = std::make_shared<std::string>();
    boost::filesystem::load_string_file(_root_dir / targetname, *image_data);
    return image_data;
  }

 private:
  TemporaryDirectory _root_dir;
  TemporaryFile _default_image_file;
  boost::filesystem::path _director_dir{_root_dir / "repo/director"};
  boost::filesystem::path _imagerepo_dir{_root_dir / "repo/repo"};
  Process _uptane_gen{UPTANE_METADATA_GENERATOR.string()};
};

class SecondaryUptaneVerificationTest : public ::testing::Test {
 protected:
  SecondaryUptaneVerificationTest() {
    _uptane_repo.addImageFile(_default_target, _secondary->getHwIdResp().ToString(),
                              _secondary->getSerialResp().ToString());
  }

  std::shared_ptr<std::string> getImageData(const std::string& targetname = _default_target) const {
    return _uptane_repo.getImageData(targetname);
  }

 protected:
  static constexpr const char* const _default_target{"defaulttarget"};
  AktualizrSecondaryPtr _secondary;
  UptaneRepo _uptane_repo;
};

TEST_F(SecondaryUptaneVerificationTest, fullUptaneVerificationPositive) {
  EXPECT_TRUE(_secondary->putMetadataResp(Metadata(_uptane_repo.getCurrentMetadata())));
  EXPECT_TRUE(_secondary->sendFirmwareResp(getImageData()));
}

TEST_F(SecondaryUptaneVerificationTest, MalformedMetadaJson) {
  {
    auto malformed_metadata = _uptane_repo.getCurrentMetadata();
    malformed_metadata.director_root[4] = 'f';

    EXPECT_FALSE(_secondary->putMetadataResp(Metadata(malformed_metadata)));
  }

  {
    auto malformed_metadata = _uptane_repo.getCurrentMetadata();
    malformed_metadata.director_targets[4] = 'f';

    EXPECT_FALSE(_secondary->putMetadataResp(Metadata(malformed_metadata)));
  }

  {
    auto malformed_metadata = _uptane_repo.getCurrentMetadata();
    malformed_metadata.image_root[4] = 'f';

    EXPECT_FALSE(_secondary->putMetadataResp(Metadata(malformed_metadata)));
  }

  {
    auto malformed_metadata = _uptane_repo.getCurrentMetadata();
    malformed_metadata.image_timestamp[4] = 'f';

    EXPECT_FALSE(_secondary->putMetadataResp(Metadata(malformed_metadata)));
  }

  {
    auto malformed_metadata = _uptane_repo.getCurrentMetadata();
    malformed_metadata.image_snapshot[4] = 'f';

    EXPECT_FALSE(_secondary->putMetadataResp(Metadata(malformed_metadata)));
  }

  {
    auto malformed_metadata = _uptane_repo.getCurrentMetadata();
    malformed_metadata.image_targets[4] = 'f';

    EXPECT_FALSE(_secondary->putMetadataResp(Metadata(malformed_metadata)));
  }
}

TEST_F(SecondaryUptaneVerificationTest, IncorrectTargetQuantity) {
  {
    // two targets for the same ECU
    _uptane_repo.addImageFile("second_target", _secondary->getHwIdResp().ToString(),
                              _secondary->getSerialResp().ToString());

    EXPECT_FALSE(_secondary->putMetadataResp(Metadata(_uptane_repo.getCurrentMetadata())));
  }

  {
    // zero targets for the ECU being tested
    auto metadata = UptaneRepo().addImageFile("mytarget", _secondary->getHwIdResp().ToString(), "non-existing-serial");

    EXPECT_FALSE(_secondary->putMetadataResp(Metadata(metadata)));
  }

  {
    // zero targets for the ECU being tested
    auto metadata = UptaneRepo().addImageFile("mytarget", "non-existig-hwid", _secondary->getSerialResp().ToString());

    EXPECT_FALSE(_secondary->putMetadataResp(Metadata(metadata)));
  }
}

TEST_F(SecondaryUptaneVerificationTest, InvalidImageFileSize) {
  EXPECT_TRUE(_secondary->putMetadataResp(Metadata(_uptane_repo.getCurrentMetadata())));
  auto image_data = getImageData();
  image_data->append("\n");
  EXPECT_FALSE(_secondary->sendFirmwareResp(image_data));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the uptane-generator utility\n";
    return EXIT_FAILURE;
  }

  UPTANE_METADATA_GENERATOR = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::error);

  return RUN_ALL_TESTS();
}
#endif
