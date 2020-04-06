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
#include "utilities/utils.h"

using ::testing::NiceMock;

class UpdateAgentMock : public FileUpdateAgent {
 public:
  UpdateAgentMock(boost::filesystem::path target_filepath, std::string target_name)
      : FileUpdateAgent(std::move(target_filepath), std::move(target_name)) {
    ON_CALL(*this, download).WillByDefault([this](const Uptane::Target& target, const std::string& data) {
      return FileUpdateAgent::download(target, data);
    });
    ON_CALL(*this, receiveData).WillByDefault([this](const Uptane::Target& target, const uint8_t* data, size_t size) {
      return FileUpdateAgent::receiveData(target, data, size);
    });
    ON_CALL(*this, install).WillByDefault([this](const Uptane::Target& target) {
      return FileUpdateAgent::install(target);
    });
  }

  MOCK_METHOD(bool, download, (const Uptane::Target& target, const std::string& data));
  MOCK_METHOD(data::ResultCode::Numeric, receiveData, (const Uptane::Target& target, const uint8_t* data, size_t size));
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
                        size_t size = 2049, bool add_and_sign_target = true, bool add_invalid_images = false,
                        size_t delta = 2) {
    const auto image_file_path = root_dir_ / targetname;
    generateRandomFile(image_file_path, size);

    uptane_repo_.addImage(image_file_path, targetname, hardware_id, "", Delegation());
    if (add_and_sign_target) {
      uptane_repo_.addTarget(targetname, hardware_id, serial, "");
      uptane_repo_.signTargets();
    }

    if (add_and_sign_target && add_invalid_images) {
      const auto smaller_image_file_path = image_file_path.string() + ".smaller";
      const auto bigger_image_file_path = image_file_path.string() + ".bigger";
      const auto broken_image_file_path = image_file_path.string() + ".broken";

      boost::filesystem::copy(image_file_path, smaller_image_file_path);
      boost::filesystem::copy(image_file_path, bigger_image_file_path);
      boost::filesystem::copy(image_file_path, broken_image_file_path);

      if (!boost::filesystem::exists(smaller_image_file_path)) {
        LOG_ERROR << "File does not exists: " << smaller_image_file_path;
      }

      boost::filesystem::resize_file(smaller_image_file_path, size - delta);
      boost::filesystem::resize_file(bigger_image_file_path, size + delta);

      std::ofstream broken_image{broken_image_file_path,
                                 std::ios_base::in | std::ios_base::out | std::ios_base::ate | std::ios_base::binary};
      unsigned char data_to_inject[]{0xFF};
      broken_image.seekp(-sizeof(data_to_inject), std::ios_base::end);
      broken_image.write(reinterpret_cast<const char*>(data_to_inject), sizeof(data_to_inject));
      broken_image.close();
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

  std::string getTargetImagePath(const std::string& targetname) const { return (root_dir_ / targetname).string(); }

  void refreshRoot(Uptane::RepositoryType repo) { uptane_repo_.refresh(repo, Uptane::Role::Root()); }

 private:
  static void generateRandomFile(const boost::filesystem::path& filepath, size_t size) {
    std::ofstream file{filepath.string(), std::ofstream::binary};

    if (!file.is_open() || !file.good()) {
      throw std::runtime_error("Failed to create a file: " + filepath.string());
    }

    const unsigned char symbols[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv";
    unsigned char cur_symbol;

    for (unsigned int ii = 0; ii < size; ++ii) {
      cur_symbol = symbols[rand() % sizeof(symbols)];
      file.put(cur_symbol);
    }

    file.close();
  }

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
    uptane_repo_.addImageFile(default_target_, secondary_->getHwId().ToString(), secondary_->getSerial().ToString(),
                              target_size, true, true, inavlid_target_size_delta);
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

  data::ResultCode::Numeric sendImageFile(std::string target_name = default_target_) {
    auto image_path = uptane_repo_.getTargetImagePath(target_name);
    size_t total_size = boost::filesystem::file_size(image_path);

    std::ifstream file{image_path};

    uint8_t buf[send_buffer_size];
    size_t read_and_send_data_size = 0;

    while (read_and_send_data_size < total_size) {
      auto read_bytes = file.readsome(reinterpret_cast<char*>(buf), sizeof(buf));
      if (read_bytes < 0) {
        file.close();
        return data::ResultCode::Numeric::kGeneralError;
      }

      auto result = secondary_->sendFirmware(buf, read_bytes);
      if (result != data::ResultCode::Numeric::kOk) {
        file.close();
        return result;
      }
      read_and_send_data_size += read_bytes;
    }

    file.close();

    data::ResultCode::Numeric result{data::ResultCode::Numeric::kGeneralError};
    if (read_and_send_data_size == total_size) {
      result = data::ResultCode::Numeric::kOk;
    }

    return result;
  }

 protected:
  static constexpr const char* const default_target_{"default-target"};
  static constexpr const char* const bigger_target_{"default-target.bigger"};
  static constexpr const char* const smaller_target_{"default-target.smaller"};
  static constexpr const char* const broken_target_{"default-target.broken"};

  static const size_t target_size{2049};
  static const size_t inavlid_target_size_delta{2};
  static const size_t send_buffer_size{1024};

  AktualizrSecondaryWrapper secondary_;
  UptaneRepoWrapper uptane_repo_;
  NiceMock<UpdateAgentMock>& update_agent_;
  TemporaryDirectory image_dir_;
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
  EXPECT_CALL(update_agent_, receiveData)
      .Times(target_size / send_buffer_size + (target_size % send_buffer_size ? 1 : 0));
  EXPECT_CALL(update_agent_, install).Times(1);

  ASSERT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
  ASSERT_EQ(sendImageFile(), data::ResultCode::Numeric::kOk);
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
                            target_size, false, false);
  EXPECT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
}

TEST_F(SecondaryTest, IncorrectTargetQuantity) {
  {
    // two targets for the same ECU
    uptane_repo_.addImageFile("second_target", secondary_->getHwId().ToString(), secondary_->getSerial().ToString());

    auto meta = uptane_repo_.getCurrentMetadata();
    EXPECT_FALSE(secondary_->putMetadata(meta));
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

TEST_F(SecondaryTest, SmallerImageFileSize) {
  EXPECT_CALL(update_agent_, receiveData)
      .Times((target_size - inavlid_target_size_delta) / send_buffer_size +
             ((target_size - inavlid_target_size_delta) % send_buffer_size ? 1 : 0));
  EXPECT_CALL(update_agent_, install).Times(1);

  EXPECT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));

  EXPECT_EQ(sendImageFile(smaller_target_), data::ResultCode::Numeric::kOk);
  EXPECT_NE(secondary_->install(default_target_), data::ResultCode::Numeric::kOk);
}

TEST_F(SecondaryTest, BiggerImageFileSize) {
  EXPECT_CALL(update_agent_, receiveData)
      .Times((target_size + inavlid_target_size_delta) / send_buffer_size +
             ((target_size + inavlid_target_size_delta) % send_buffer_size ? 1 : 0));
  EXPECT_CALL(update_agent_, install).Times(1);

  EXPECT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));

  EXPECT_EQ(sendImageFile(bigger_target_), data::ResultCode::Numeric::kOk);
  EXPECT_NE(secondary_->install(default_target_), data::ResultCode::Numeric::kOk);
}

TEST_F(SecondaryTest, InvalidImageData) {
  EXPECT_CALL(update_agent_, receiveData)
      .Times(target_size / send_buffer_size + (target_size % send_buffer_size ? 1 : 0));
  EXPECT_CALL(update_agent_, install).Times(1);

  EXPECT_TRUE(secondary_->putMetadata(uptane_repo_.getCurrentMetadata()));
  EXPECT_EQ(sendImageFile(broken_target_), data::ResultCode::Numeric::kOk);
  EXPECT_NE(secondary_->install(default_target_), data::ResultCode::Numeric::kOk);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::info);

  return RUN_ALL_TESTS();
}
