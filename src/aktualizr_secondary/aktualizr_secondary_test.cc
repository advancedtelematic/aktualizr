#include <gtest/gtest.h>

#include <boost/process.hpp>

#include "aktualizr_secondary.h"
#include "aktualizr_secondary_factory.h"
#include "test_utils.h"
#include "uptane_repo.h"

class AktualizrSecondaryWrapper {
 public:
  AktualizrSecondaryWrapper() {
    AktualizrSecondaryConfig config;
    config.pacman.type = PACKAGE_MANAGER_NONE;

    config.storage.path = _storage_dir.Path();
    config.storage.type = StorageType::kSqlite;

    _storage = INvStorage::newStorage(config.storage);
    _secondary = AktualizrSecondaryFactory::create(config, _storage);
  }

 public:
  std::shared_ptr<AktualizrSecondary>& operator->() { return _secondary; }

  Uptane::Target getPendingVersion() const {
    boost::optional<Uptane::Target> pending_target;

    _storage->loadInstalledVersions(_secondary->getSerial().ToString(), nullptr, &pending_target);
    return *pending_target;
  }

  std::string hardwareID() const { return _secondary->getHwId().ToString(); }

  std::string serial() const { return _secondary->getSerial().ToString(); }

  boost::filesystem::path targetFilepath() const {
    return _storage_dir.Path() / AktualizrSecondaryFactory::BinaryUpdateDefaultFile;
  }

 private:
  TemporaryDirectory _storage_dir;
  AktualizrSecondary::Ptr _secondary;
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

  std::string getImageData(const std::string& targetname) const {
    std::string image_data;
    boost::filesystem::load_string_file(_root_dir / targetname, image_data);
    return image_data;
  }

  void refreshRoot(Uptane::RepositoryType repo) { _uptane_repo.refresh(repo, Uptane::Role::Root()); }

 private:
  TemporaryDirectory _root_dir;
  boost::filesystem::path _director_dir{_root_dir / "repo/director"};
  boost::filesystem::path _imagerepo_dir{_root_dir / "repo/repo"};
  UptaneRepo _uptane_repo{_root_dir.Path(), "", ""};
  Uptane::DirectorRepository _director_repo;
};

class SecondaryTest : public ::testing::Test {
 protected:
  SecondaryTest() {
    _uptane_repo.addImageFile(_default_target, _secondary->getHwId().ToString(), _secondary->getSerial().ToString());
  }

  std::string getImageData(const std::string& targetname = _default_target) const {
    return _uptane_repo.getImageData(targetname);
  }

  std::vector<Uptane::Target> getCurrentTargets() {
    auto targets = Uptane::Targets(Utils::parseJSON(_uptane_repo.getCurrentMetadata().director_targets));
    return targets.getTargets(_secondary->getSerial(), _secondary->getHwId());
  }

  Uptane::Target getDefaultTarget() {
    auto targets = getCurrentTargets();
    EXPECT_GT(targets.size(), 0);
    return targets[0];
  }

  Uptane::Hash getDefaultTargetHash() {
    return Uptane::Hash(Uptane::Hash::Type::kSha256, getDefaultTarget().sha256Hash());
  }

 protected:
  static constexpr const char* const _default_target{"default-target"};
  AktualizrSecondaryWrapper _secondary;
  UptaneRepoWrapper _uptane_repo;
};

class SecondaryTestNegative : public ::testing::Test,
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

 protected:
  static AktualizrSecondaryWrapper _secondary;
  static UptaneRepoWrapper _uptane_repo;
};

AktualizrSecondaryWrapper SecondaryTestNegative::_secondary;
UptaneRepoWrapper SecondaryTestNegative::_uptane_repo;

/**
 * Parameterized test,
 * The parameter is std::pair<Uptane::RepositoryType, Uptane::Role> to indicate which metadata to malform
 *
 * see INSTANTIATE_TEST_SUITE_P for the test instantiations with concrete parameter values
 */
TEST_P(SecondaryTestNegative, MalformedMetadaJson) { EXPECT_FALSE(_secondary->putMetadata(currentMetadata())); }

/**
 * Instantiates the parameterized test for each specified value of std::pair<Uptane::RepositoryType, Uptane::Role>
 * the parameter value indicates which metadata to malform
 */
INSTANTIATE_TEST_SUITE_P(SecondaryTestMalformedMetadata, SecondaryTestNegative,
                         ::testing::Values(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Root()),
                                           std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Targets()),
                                           std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Root()),
                                           std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()),
                                           std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()),
                                           std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Targets())));

TEST_F(SecondaryTest, fullUptaneVerificationPositive) {
  ASSERT_TRUE(_secondary->putMetadata(_uptane_repo.getCurrentMetadata()));
  ASSERT_TRUE(_secondary->sendFirmware(getImageData()));
  ASSERT_EQ(_secondary->install(_default_target), data::ResultCode::Numeric::kOk);

  // check if a file was actually updated
  ASSERT_TRUE(boost::filesystem::exists(_secondary.targetFilepath()));
  auto target = getDefaultTarget();

  // check the updated file hash
  auto target_hash = Uptane::Hash(Uptane::Hash::Type::kSha256, target.sha256Hash());
  auto target_file_hash =
      Uptane::Hash::generate(Uptane::Hash::Type::kSha256, Utils::readFile(_secondary.targetFilepath()));
  EXPECT_EQ(target_hash, target_file_hash);

  // check the secondary manifest
  auto manifest = _secondary->getManifest();
  EXPECT_EQ(manifest.installedImageHash(), target_file_hash);
  EXPECT_EQ(manifest.filepath(), target.filename());
}

TEST_F(SecondaryTest, TwoImagesAndOneTarget) {
  // two images for the same ECU, just one of them is added as a target and signed
  // default image and corresponding target has been already added, just add another image
  _uptane_repo.addImageFile("second_image_00", _secondary->getHwId().ToString(), _secondary->getSerial().ToString(),
                            false);
  EXPECT_TRUE(_secondary->putMetadata(_uptane_repo.getCurrentMetadata()));
}

TEST_F(SecondaryTest, IncorrectTargetQuantity) {
  {
    // two targets for the same ECU
    _uptane_repo.addImageFile("second_target", _secondary->getHwId().ToString(), _secondary->getSerial().ToString());

    EXPECT_FALSE(_secondary->putMetadata(_uptane_repo.getCurrentMetadata()));
  }

  {
    // zero targets for the ECU being tested
    auto metadata =
        UptaneRepoWrapper().addImageFile("mytarget", _secondary->getHwId().ToString(), "non-existing-serial");

    EXPECT_FALSE(_secondary->putMetadata(metadata));
  }

  {
    // zero targets for the ECU being tested
    auto metadata =
        UptaneRepoWrapper().addImageFile("mytarget", "non-existig-hwid", _secondary->getSerial().ToString());

    EXPECT_FALSE(_secondary->putMetadata(metadata));
  }
}

TEST_F(SecondaryTest, DirectorRootVersionIncremented) {
  _uptane_repo.refreshRoot(Uptane::RepositoryType::Director());
  EXPECT_TRUE(_secondary->putMetadata(_uptane_repo.getCurrentMetadata()));
}

TEST_F(SecondaryTest, ImageRootVersionIncremented) {
  _uptane_repo.refreshRoot(Uptane::RepositoryType::Image());
  EXPECT_TRUE(_secondary->putMetadata(_uptane_repo.getCurrentMetadata()));
}

TEST_F(SecondaryTest, InvalidImageFileSize) {
  EXPECT_TRUE(_secondary->putMetadata(_uptane_repo.getCurrentMetadata()));
  auto image_data = getImageData();
  image_data.append("\n");
  EXPECT_FALSE(_secondary->sendFirmware(image_data));
}

TEST_F(SecondaryTest, InvalidImageData) {
  EXPECT_TRUE(_secondary->putMetadata(_uptane_repo.getCurrentMetadata()));
  auto image_data = getImageData();
  image_data.operator[](3) = '0';
  EXPECT_FALSE(_secondary->sendFirmware(image_data));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::info);

  return RUN_ALL_TESTS();
}
