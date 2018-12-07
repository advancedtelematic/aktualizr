#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <boost/filesystem.hpp>

#include "config/config.h"
#include "logging/logging.h"
#include "test_utils.h"
#include "uptane_repo.h"

KeyType key_type = KeyType::kUnknown;

TEST(aktualizr_repo, generate_repo) {
  TemporaryDirectory temp_dir;
  UptaneRepo repo(temp_dir.Path(), "", "correlation");
  repo.generateRepo(key_type);
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/director/root.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/director/targets.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/director/timestamp.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/image/root.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/image/targets.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/image/timestamp.json"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "repo/director/manifest"));

  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/root/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/root/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/snapshot/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/snapshot/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/targets/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/targets/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/timestamp/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/image/timestamp/public.key"));

  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/root/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/root/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/snapshot/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/snapshot/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/targets/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/targets/public.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/timestamp/private.key"));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / "keys/director/timestamp/public.key"));

  Json::Value image_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(image_targets["signed"]["targets"].size(), 0);
  Json::Value director_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/targets.json");
  EXPECT_EQ(director_targets["signed"]["targets"].size(), 0);
  EXPECT_EQ(director_targets["signed"]["custom"]["correlationId"], "correlation");
}

TEST(aktualizr_repo, add_image) {
  TemporaryDirectory temp_dir;
  UptaneRepo repo(temp_dir.Path(), "", "");
  repo.generateRepo(key_type);
  repo.addImage(temp_dir.Path() / "repo/director/manifest");
  Json::Value image_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(image_targets["signed"]["targets"].size(), 1);
  Json::Value director_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/targets.json");
  EXPECT_EQ(director_targets["signed"]["targets"].size(), 0);
}

TEST(aktualizr_repo, copy_image) {
  TemporaryDirectory temp_dir;
  UptaneRepo repo(temp_dir.Path(), "", "");
  repo.generateRepo(key_type);
  repo.addImage(temp_dir.Path() / "repo/director/manifest");
  repo.addTarget("manifest", "test-hw", "test-serial");
  repo.signTargets();
  Json::Value image_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/image/targets.json");
  EXPECT_EQ(image_targets["signed"]["targets"].size(), 1);
  Json::Value director_targets = Utils::parseJSONFile(temp_dir.Path() / "repo/director/targets.json");
  EXPECT_EQ(director_targets["signed"]["targets"].size(), 1);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  bool res = true;
  for (int type = static_cast<int>(KeyType::kFirstKnown); type <= static_cast<int>(KeyType::kLastKnown); type++) {
    LOG_TRACE << "Running tests for key type " << static_cast<KeyType>(type);
    key_type = static_cast<KeyType>(type);
    res &= RUN_ALL_TESTS();
  }
  return res;
}
#endif
