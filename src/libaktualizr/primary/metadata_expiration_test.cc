#include <gtest/gtest.h>

#include <string>

#include "httpfake.h"
#include "libaktualizr/aktualizr.h"
#include "test_utils.h"
#include "uptane_test_common.h"

boost::filesystem::path uptane_generator_path;

class MetadataExpirationTest : public ::testing::Test {
 protected:
  MetadataExpirationTest() : uptane_gen_(uptane_generator_path.string()) {
    Process uptane_gen(uptane_generator_path.string());
    uptane_gen.run({"generate", "--path", meta_dir_.PathString()});

    auto http = std::make_shared<HttpFake>(temp_dir_.Path(), "", meta_dir_.Path() / "repo");
    Config conf = UptaneTestCommon::makeTestConfig(temp_dir_, http->tls_server);
    conf.pacman.fake_need_reboot = true;
    conf.uptane.force_install_completion = true;
    conf.bootloader.reboot_sentinel_dir = temp_dir_.Path();

    logger_set_threshold(boost::log::trivial::trace);

    storage_ = INvStorage::newStorage(conf.storage);
    aktualizr_ = std::make_shared<UptaneTestCommon::TestAktualizr>(conf, storage_, http);
  }

  void addImage() {
    uptane_gen_.run({"image", "--path", meta_dir_.PathString(), "--filename", "tests/test_data/firmware.txt",
                     "--targetname", target_filename_, "--hwid", "primary_hw"});

    target_image_hash_ = boost::algorithm::to_lower_copy(
        boost::algorithm::hex(Crypto::sha256digest(Utils::readFile("tests/test_data/firmware.txt"))));
  }

  void addTarget(const std::string& target_filename, int expiration_delta = 0) {
    if (expiration_delta != 0) {
      time_t new_expiration_time;
      std::time(&new_expiration_time);
      new_expiration_time += expiration_delta;
      struct tm new_expiration_time_str {};
      gmtime_r(&new_expiration_time, &new_expiration_time_str);

      uptane_gen_.run({"addtarget", "--path", meta_dir_.PathString(), "--targetname", target_filename, "--hwid",
                       "primary_hw", "--serial", "CA:FE:A6:D2:84:9D", "--expires",
                       TimeStamp(new_expiration_time_str).ToString()});

    } else {
      uptane_gen_.run({"addtarget", "--path", meta_dir_.PathString(), "--targetname", target_filename, "--hwid",
                       "primary_hw", "--serial", "CA:FE:A6:D2:84:9D"});
    }
  }

  void addTargetAndSign(const std::string& target_filename, int expiration_delta = 0) {
    addTarget(target_filename, expiration_delta);
    uptane_gen_.run({"signtargets", "--path", meta_dir_.PathString()});
  }

  void refreshTargetsMetadata() {
    // refresh the Targets metadata in the repo/Director
    uptane_gen_.run({"refresh", "--path", meta_dir_.PathString(), "--repotype", "director", "--keyname", "targets"});
  }

  void addTargetToInstall(int expiration_delta = 0) {
    addImage();
    addTargetAndSign(target_filename_, expiration_delta);
  }

 protected:
  Process uptane_gen_;
  const std::string target_filename_ = "firmware.txt";
  std::string target_image_hash_;

  TemporaryDirectory meta_dir_;
  TemporaryDirectory temp_dir_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<UptaneTestCommon::TestAktualizr> aktualizr_;
};

TEST_F(MetadataExpirationTest, MetadataExpirationBeforeInstallation) {
  aktualizr_->Initialize();
  result::UpdateCheck update_result = aktualizr_->CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);

  addTargetToInstall(-1);

  // run the uptane cycle an try to install the target
  aktualizr_->UptaneCycle();

  // check if the target has been installed and pending to be applied after a reboot
  auto& client = aktualizr_->uptane_client();
  ASSERT_FALSE(client->hasPendingUpdates());
  ASSERT_FALSE(client->isInstallCompletionRequired());

  auto currently_installed_target = client->getCurrent();

  EXPECT_NE(target_image_hash_, currently_installed_target.sha256Hash());
  EXPECT_NE(target_filename_, currently_installed_target.filename());

  refreshTargetsMetadata();

  // run the uptane cycle an try to install the target
  aktualizr_->UptaneCycle();

  // check if the target has been installed and pending to be applied after a reboot
  ASSERT_TRUE(client->hasPendingUpdates());
  ASSERT_TRUE(client->isInstallCompletionRequired());

  // force reboot
  client->completeInstall();

  // emulate aktualizr fresh start
  aktualizr_->Initialize();
  aktualizr_->UptaneCycle();

  ASSERT_FALSE(client->hasPendingUpdates());
  ASSERT_FALSE(client->isInstallCompletionRequired());

  currently_installed_target = client->getCurrent();
  EXPECT_EQ(target_image_hash_, currently_installed_target.sha256Hash());
  EXPECT_EQ(target_filename_, currently_installed_target.filename());
}

TEST_F(MetadataExpirationTest, MetadataExpirationAfterInstallationAndBeforeReboot) {
  aktualizr_->Initialize();

  result::UpdateCheck update_result = aktualizr_->CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);

  const int expiration_in_sec = 5;
  addTargetToInstall(expiration_in_sec);
  auto target_init_time = std::chrono::system_clock::now();

  // run the uptane cycle to install the target
  aktualizr_->UptaneCycle();

  // check if the target has been installed and pending to be applied after a reboot
  auto& client = aktualizr_->uptane_client();
  ASSERT_TRUE(client->hasPendingUpdates());
  ASSERT_TRUE(client->isInstallCompletionRequired());

  // emulate the target metadata expiration while the uptane cycle is running
  std::this_thread::sleep_for(std::chrono::seconds(expiration_in_sec) -
                              (std::chrono::system_clock::now() - target_init_time));
  aktualizr_->UptaneCycle();

  // since the installation happenned before the metadata expiration we expect that
  // the update is still pending and will be applied after a reboot
  ASSERT_TRUE(client->hasPendingUpdates());
  ASSERT_TRUE(client->isInstallCompletionRequired());

  // force reboot
  client->completeInstall();

  // emulate aktualizr fresh start
  aktualizr_->Initialize();
  aktualizr_->UptaneCycle();

  // check if the pending target has been applied. it should be applied in even if it's metadata are expired
  // as long as it was installed at the moment when they were not expired
  auto currently_installed_target = client->getCurrent();
  EXPECT_EQ(target_image_hash_, currently_installed_target.sha256Hash());
  EXPECT_EQ(target_filename_, currently_installed_target.filename());
}

TEST_F(MetadataExpirationTest, MetadataExpirationAfterInstallationAndBeforeApplication) {
  aktualizr_->Initialize();

  result::UpdateCheck update_result = aktualizr_->CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);

  const int expiration_in_sec = 5;
  addTargetToInstall(expiration_in_sec);
  auto target_init_time = std::chrono::system_clock::now();

  // run the uptane cycle to install the target
  aktualizr_->UptaneCycle();

  // check if the target has been installed and pending to be applied after a reboot
  auto& client = aktualizr_->uptane_client();
  ASSERT_TRUE(client->hasPendingUpdates());
  ASSERT_TRUE(client->isInstallCompletionRequired());

  // wait until the target metadata are expired
  // emulate the target metadata expiration while the Uptane cycle is running
  std::this_thread::sleep_for(std::chrono::seconds(expiration_in_sec) -
                              (std::chrono::system_clock::now() - target_init_time));

  // force reboot
  client->completeInstall();

  // emulate aktualizr fresh start
  aktualizr_->Initialize();
  aktualizr_->UptaneCycle();

  // check if the pending target has been applied. it should be applied in even if it's metadta are expired
  // as long as it was installed at the moment when they were not expired
  auto currently_installed_target = client->getCurrent();
  EXPECT_EQ(target_image_hash_, currently_installed_target.sha256Hash());
  EXPECT_EQ(target_filename_, currently_installed_target.filename());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the uptane-generator utility\n";
    return EXIT_FAILURE;
  }
  uptane_generator_path = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
