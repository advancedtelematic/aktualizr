#include <gtest/gtest.h>

#include <boost/process.hpp>
#include "aktualizr_secondary.h"
#include "test_utils.h"
#include "uptane_repo.h"

class AktualizrSecondaryWrapper {
 public:
  AktualizrSecondaryWrapper() {
    // binary update
    AktualizrSecondaryConfig config;
    config.pacman.type = PackageManager::kNone;

    config.storage.path = _storage_dir.Path();
    config.storage.type = StorageType::kSqlite;

    auto storage = INvStorage::newStorage(config.storage);
    _secondary = std::make_shared<AktualizrSecondary>(config, storage);
  }

 public:
  std::shared_ptr<AktualizrSecondary>& operator->() { return _secondary; }

  Uptane::Target getPendingVersion() const {
    boost::optional<Uptane::Target> pending_target;

    _storage->loadInstalledVersions(_secondary->getSerialResp().ToString(), nullptr, &pending_target);
    return *pending_target;
  }

  std::string hardwareID() const { return _secondary->getHwIdResp().ToString(); }

  std::string serial() const { return _secondary->getSerialResp().ToString(); }

 private:
  TemporaryDirectory _storage_dir;
  std::shared_ptr<AktualizrSecondary> _secondary;
  std::shared_ptr<INvStorage> _storage;
};

class UptaneRepoWrapper {
 public:
  UptaneRepoWrapper() { _uptane_repo.generateRepo(KeyType::kED25519); }

  Metadata addImageFile(const std::string& targetname, const std::string& hardware_id, const std::string& serial,
                        bool add_and_sign_target = true) {
    const auto image_file_path = _root_dir / targetname;
    boost::filesystem::ofstream(image_file_path) << "some data";

    _uptane_repo.addImage(image_file_path, targetname, hardware_id, "", Delegation());
    if (add_and_sign_target) {
      _uptane_repo.addTarget(targetname, hardware_id, serial, "");
      _uptane_repo.signTargets();
    }

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
  boost::filesystem::path _director_dir{_root_dir / "repo/director"};
  boost::filesystem::path _imagerepo_dir{_root_dir / "repo/repo"};
  UptaneRepo _uptane_repo{_root_dir.Path(), "", ""};
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
  AktualizrSecondaryWrapper _secondary;
  UptaneRepoWrapper _uptane_repo;
};

class SecondaryUptaneVerificationTestNegative
    : public SecondaryUptaneVerificationTest,
      public ::testing::WithParamInterface<std::pair<Uptane::RepositoryType, Uptane::Role>> {
 protected:
  class MetadataInvalidator : public Metadata {
   public:
    MetadataInvalidator(const Uptane::RawMetaPack& valid_metadata, const Uptane::RepositoryType& repo,
                        const Uptane::Role& role)
        : Metadata(valid_metadata), _repo_type(repo), _role(role) {}

    bool getRoleMetadata(std::string* result, const Uptane::RepositoryType& repo, const Uptane::Role& role,
                         Uptane::Version version) const override {
      auto return_val = Metadata::getRoleMetadata(result, repo, role, version);
      if (!(_repo_type == repo && _role == role)) {
        return return_val;
      }
      (*result)[10] = 'f';
      return true;
    }

   private:
    Uptane::RepositoryType _repo_type;
    Uptane::Role _role;
  };

 protected:
  MetadataInvalidator currentMetadata() const {
    return MetadataInvalidator(_uptane_repo.getCurrentMetadata(), GetParam().first, GetParam().second);
  }
};

/**
 * Parameterized test,
 * The parameter is std::pair<Uptane::RepositoryType, Uptane::Role> to indicate which metadata to malform
 *
 * see INSTANTIATE_TEST_SUITE_P for the test instantiations with concrete parameter values
 */
TEST_P(SecondaryUptaneVerificationTestNegative, MalformedMetadaJson) {
  EXPECT_FALSE(_secondary->putMetadataResp(currentMetadata()));
}

/**
 * Instantiates the parameterized test for each specified value of std::pair<Uptane::RepositoryType, Uptane::Role>
 * the parameter value indicates which metadata to malform
 */
INSTANTIATE_TEST_SUITE_P(SecondaryUptaneVerificationMalformedMetadata, SecondaryUptaneVerificationTestNegative,
                         ::testing::Values(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Root()),
                                           std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Targets()),
                                           std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Root()),
                                           std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()),
                                           std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()),
                                           std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Targets())));

TEST_F(SecondaryUptaneVerificationTest, fullUptaneVerificationPositive) {
  EXPECT_TRUE(_secondary->putMetadataResp(_uptane_repo.getCurrentMetadata()));
  EXPECT_TRUE(_secondary->sendFirmwareResp(getImageData()));
  EXPECT_EQ(_secondary->installResp(_default_target), data::ResultCode::Numeric::kOk);
}

TEST_F(SecondaryUptaneVerificationTest, TwoImagesAndOneTarget) {
  // two images for the same ECU, just one of them is added as a target and signed
  // default image and corresponding target has been already added, just add another image
  _uptane_repo.addImageFile("second_image_00", _secondary->getHwIdResp().ToString(),
                            _secondary->getSerialResp().ToString(), false);
  EXPECT_TRUE(_secondary->putMetadataResp(_uptane_repo.getCurrentMetadata()));
}

TEST_F(SecondaryUptaneVerificationTest, IncorrectTargetQuantity) {
  {
    // two targets for the same ECU
    _uptane_repo.addImageFile("second_target", _secondary->getHwIdResp().ToString(),
                              _secondary->getSerialResp().ToString());

    EXPECT_FALSE(_secondary->putMetadataResp(_uptane_repo.getCurrentMetadata()));
  }

  {
    // zero targets for the ECU being tested
    auto metadata =
        UptaneRepoWrapper().addImageFile("mytarget", _secondary->getHwIdResp().ToString(), "non-existing-serial");

    EXPECT_FALSE(_secondary->putMetadataResp(metadata));
  }

  {
    // zero targets for the ECU being tested
    auto metadata =
        UptaneRepoWrapper().addImageFile("mytarget", "non-existig-hwid", _secondary->getSerialResp().ToString());

    EXPECT_FALSE(_secondary->putMetadataResp(metadata));
  }
}

TEST_F(SecondaryUptaneVerificationTest, InvalidImageFileSize) {
  EXPECT_TRUE(_secondary->putMetadataResp(_uptane_repo.getCurrentMetadata()));
  auto image_data = getImageData();
  image_data->append("\n");
  EXPECT_FALSE(_secondary->sendFirmwareResp(image_data));
}

TEST_F(SecondaryUptaneVerificationTest, InvalidImageData) {
  EXPECT_TRUE(_secondary->putMetadataResp(_uptane_repo.getCurrentMetadata()));
  auto image_data = getImageData();
  image_data->operator[](3) = '0';
  EXPECT_FALSE(_secondary->sendFirmwareResp(image_data));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::info);

  return RUN_ALL_TESTS();
}
#endif
