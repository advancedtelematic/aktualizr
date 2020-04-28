#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/process.hpp>

#include "aktualizr_secondary.h"
#include "aktualizr_secondary_factory.h"
#include "crypto/keymanager.h"
#include "test_utils.h"
#include "update_agent.h"
#include "update_agent_file.h"
#include "uptane_repo.h"

using ::testing::NiceMock;

class UpdateAgentMock : public FileUpdateAgent {
 public:
  UpdateAgentMock(boost::filesystem::path target_filepath, std::string target_name)
      : FileUpdateAgent(std::move(target_filepath), std::move(target_name)) {
    ON_CALL(*this, download).WillByDefault([this](const Uptane::Target& target, const std::string& data) {
      return FileUpdateAgent::download(target, data);
    });
    ON_CALL(*this, install).WillByDefault([this](const Uptane::Target& target) {
      return FileUpdateAgent::install(target);
    });
  }

  MOCK_METHOD(bool, download, (const Uptane::Target& target, const std::string& data));
  MOCK_METHOD(data::ResultCode::Numeric, install, (const Uptane::Target& target));
};

class AktualizrSecondaryWrapper {
 public:
  AktualizrSecondaryWrapper() {
    AktualizrSecondaryConfig config;
    config.pacman.type = PACKAGE_MANAGER_NONE;

    config.storage.path = storage_dir_.Path();
    config.storage.type = StorageType::kSqlite;

    storage_ = INvStorage::newStorage(config.storage);
    auto key_mngr = std::make_shared<KeyManager>(storage_, config.keymanagerConfig());
    update_agent = std::make_shared<NiceMock<UpdateAgentMock>>(config.storage.path / "firmware.txt", "");

    secondary_ = std::make_shared<AktualizrSecondary>(config, storage_, key_mngr, update_agent);
  }

  std::shared_ptr<AktualizrSecondary>& operator->() { return secondary_; }

  Uptane::Target getPendingVersion() const {
    boost::optional<Uptane::Target> pending_target;

    storage_->loadInstalledVersions(secondary_->getSerial().ToString(), nullptr, &pending_target);
    return *pending_target;
  }

  std::string hardwareID() const { return secondary_->getHwId().ToString(); }

  std::string serial() const { return secondary_->getSerial().ToString(); }

  boost::filesystem::path targetFilepath() const {
    return storage_dir_.Path() / AktualizrSecondaryFactory::BinaryUpdateDefaultFile;
  }

  std::shared_ptr<NiceMock<UpdateAgentMock>> update_agent;

 private:
  TemporaryDirectory storage_dir_;
  AktualizrSecondary::Ptr secondary_;
  std::shared_ptr<INvStorage> storage_;
};

class UptaneRepoWrapper {
 public:
  UptaneRepoWrapper() { uptane_repo_.generateRepo(KeyType::kED25519); }

  Metadata addImageFile(const std::string& targetname, const std::string& hardware_id, const std::string& serial,
                        bool add_and_sign_target = true) {
    const auto image_file_path = root_dir_ / targetname;
    boost::filesystem::ofstream(image_file_path) << "some data";

    uptane_repo_.addImage(image_file_path, targetname, hardware_id, "", Delegation());
    if (add_and_sign_target) {
      uptane_repo_.addTarget(targetname, hardware_id, serial, "");
      uptane_repo_.signTargets();
    }

    return getCurrentMetadata();
  }

  Uptane::RawMetaPack getCurrentMetadata() const {
    Uptane::RawMetaPack metadata;

    boost::filesystem::load_string_file(director_dir_ / "root.json", metadata.director_root);
    boost::filesystem::load_string_file(director_dir_ / "targets.json", metadata.director_targets);

    boost::filesystem::load_string_file(imagerepo_dir_ / "root.json", metadata.image_root);
    boost::filesystem::load_string_file(imagerepo_dir_ / "timestamp.json", metadata.image_timestamp);
    boost::filesystem::load_string_file(imagerepo_dir_ / "snapshot.json", metadata.image_snapshot);
    boost::filesystem::load_string_file(imagerepo_dir_ / "targets.json", metadata.image_targets);

    return metadata;
  }

  std::string getImageData(const std::string& targetname) const {
    std::string image_data;
    boost::filesystem::load_string_file(root_dir_ / targetname, image_data);
    return image_data;
  }

  void refreshRoot(Uptane::RepositoryType repo) { uptane_repo_.refresh(repo, Uptane::Role::Root()); }

 private:
  TemporaryDirectory root_dir_;
  boost::filesystem::path director_dir_{root_dir_ / "repo/director"};
  boost::filesystem::path imagerepo_dir_{root_dir_ / "repo/repo"};
  UptaneRepo uptane_repo_{root_dir_.Path(), "", ""};
  Uptane::DirectorRepository director_repo_;
};

class SecondaryTest : public ::testing::Test {
 protected:
  SecondaryTest() : update_agent_(*(secondary_.update_agent)) {
    uptane_repo_.addImageFile(default_target_, secondary_->getHwId().ToString(), secondary_->getSerial().ToString());
  }

  std::string getImageData(const std::string& targetname = default_target_) const {
    return uptane_repo_.getImageData(targetname);
  }

  std::vector<Uptane::Target> getCurrentTargets() {
    auto targets = Uptane::Targets(Utils::parseJSON(uptane_repo_.getCurrentMetadata().director_targets));
    return targets.getTargets(secondary_->getSerial(), secondary_->getHwId());
  }

  Uptane::Target getDefaultTarget() {
    auto targets = getCurrentTargets();
    EXPECT_GT(targets.size(), 0);
    return targets[0];
  }

  Hash getDefaultTargetHash() { return Hash(Hash::Type::kSha256, getDefaultTarget().sha256Hash()); }

 protected:
  static constexpr const char* const default_target_{"default-target"};
  AktualizrSecondaryWrapper secondary_;
  UptaneRepoWrapper uptane_repo_;
  NiceMock<UpdateAgentMock>& update_agent_;
};

class SecondaryTestNegative : public ::testing::Test,
                              public ::testing::WithParamInterface<std::pair<Uptane::RepositoryType, Uptane::Role>> {
 public:
  SecondaryTestNegative() : update_agent_(*(secondary_.update_agent)) {}

 protected:
  class MetadataInvalidator : public Metadata {
   public:
    MetadataInvalidator(const Uptane::RawMetaPack& valid_metadata, const Uptane::RepositoryType& repo,
                        const Uptane::Role& role)
        : Metadata(valid_metadata), repo_type_(repo), role_(role) {}

    void getRoleMetadata(std::string* result, const Uptane::RepositoryType& repo, const Uptane::Role& role,
                         Uptane::Version version) const override {
      Metadata::getRoleMetadata(result, repo, role, version);
      if (!(repo_type_ == repo && role_ == role)) {
        return;
      }
      (*result)[10] = 'f';
    }

   private:
    Uptane::RepositoryType repo_type_;
    Uptane::Role role_;
  };

  MetadataInvalidator currentMetadata() const {
    return MetadataInvalidator(uptane_repo_.getCurrentMetadata(), GetParam().first, GetParam().second);
  }

  AktualizrSecondaryWrapper secondary_;
  UptaneRepoWrapper uptane_repo_;
  NiceMock<UpdateAgentMock>& update_agent_;
};

/**
 * Parameterized test,
 * The parameter is std::pair<Uptane::RepositoryType, Uptane::Role> to indicate which metadata to malform
 *
 * see INSTANTIATE_TEST_SUITE_P for the test instantiations with concrete parameter values
 */
TEST_P(SecondaryTestNegative, MalformedMetadaJson) {
  EXPECT_FALSE(secondary_->putMetadata(currentMetadata()));

  EXPECT_CALL(update_agent_, download).Times(0);
  EXPECT_CALL(update_agent_, install).Times(0);

  EXPECT_FALSE(secondary_->sendFirmware("firmware"));

  EXPECT_NE(secondary_->install("target"), data::ResultCode::Numeric::kOk);
}

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
  EXPECT_CALL(update_agent_, download).Times(1);
  EXPECT_CALL(update_agent_, install).Times(1);

  ASSERT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
  ASSERT_TRUE(secondary_->sendFirmware(getImageData()));
  ASSERT_EQ(secondary_->install(default_target_), data::ResultCode::Numeric::kOk);

  // check if a file was actually updated
  ASSERT_TRUE(boost::filesystem::exists(secondary_.targetFilepath()));
  auto target = getDefaultTarget();

  // check the updated file hash
  auto target_hash = Hash(Hash::Type::kSha256, target.sha256Hash());
  auto target_file_hash = Hash::generate(Hash::Type::kSha256, Utils::readFile(secondary_.targetFilepath()));
  EXPECT_EQ(target_hash, target_file_hash);

  // check the secondary manifest
  auto manifest = secondary_->getManifest();
  EXPECT_EQ(manifest.installedImageHash(), target_file_hash);
  EXPECT_EQ(manifest.filepath(), target.filename());
}

TEST_F(SecondaryTest, TwoImagesAndOneTarget) {
  // two images for the same ECU, just one of them is added as a target and signed
  // default image and corresponding target has been already added, just add another image
  uptane_repo_.addImageFile("second_image_00", secondary_->getHwId().ToString(), secondary_->getSerial().ToString(),
                            false);
  EXPECT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
}

TEST_F(SecondaryTest, IncorrectTargetQuantity) {
  {
    // two targets for the same ECU
    uptane_repo_.addImageFile("second_target", secondary_->getHwId().ToString(), secondary_->getSerial().ToString());

    EXPECT_FALSE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
  }

  {
    // zero targets for the ECU being tested
    auto metadata =
        UptaneRepoWrapper().addImageFile("mytarget", secondary_->getHwId().ToString(), "non-existing-serial");

    EXPECT_FALSE(secondary_->putMetadata(metadata));
  }

  {
    // zero targets for the ECU being tested
    auto metadata =
        UptaneRepoWrapper().addImageFile("mytarget", "non-existig-hwid", secondary_->getSerial().ToString());

    EXPECT_FALSE(secondary_->putMetadata(metadata));
  }
}

TEST_F(SecondaryTest, DirectorRootVersionIncremented) {
  uptane_repo_.refreshRoot(Uptane::RepositoryType::Director());
  EXPECT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
}

TEST_F(SecondaryTest, ImageRootVersionIncremented) {
  uptane_repo_.refreshRoot(Uptane::RepositoryType::Image());
  EXPECT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
}

TEST_F(SecondaryTest, InvalidImageFileSize) {
  EXPECT_CALL(update_agent_, download).Times(1);
  EXPECT_CALL(update_agent_, install).Times(0);

  EXPECT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
  auto image_data = getImageData();
  image_data.append("\n");
  EXPECT_FALSE(secondary_->sendFirmware(image_data));
  EXPECT_NE(secondary_->install(default_target_), data::ResultCode::Numeric::kOk);
}

TEST_F(SecondaryTest, InvalidImageData) {
  EXPECT_CALL(update_agent_, download).Times(1);
  EXPECT_CALL(update_agent_, install).Times(0);

  EXPECT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
  auto image_data = getImageData();
  image_data.operator[](3) = '0';
  EXPECT_FALSE(secondary_->sendFirmware(image_data));
  EXPECT_NE(secondary_->install(default_target_), data::ResultCode::Numeric::kOk);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::info);

  return RUN_ALL_TESTS();
}
