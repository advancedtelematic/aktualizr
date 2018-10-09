#include <gtest/gtest.h>

#include <json/json.h>

#include "logging/logging.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"
#include "utilities/utils.h"

TEST(Root, RootValidates) {
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/sample1/root.json");
  LOG_INFO << "Root is:" << initial_root;

  Uptane::Root root1(Uptane::Root::Policy::kAcceptAll);
  Uptane::Root root(Uptane::RepositoryType::Director, initial_root, root1);

  EXPECT_NO_THROW(Uptane::Root(Uptane::RepositoryType::Director, initial_root, root));
}

TEST(Root, RootJsonNoKeys) {
  Uptane::Root root1(Uptane::Root::Policy::kAcceptAll);
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/sample1/root.json");
  initial_root["signed"].removeMember("keys");
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director, initial_root, root1), Uptane::InvalidMetadata);
}

TEST(Root, RootJsonNoRoles) {
  Uptane::Root root1(Uptane::Root::Policy::kAcceptAll);
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/sample1/root.json");
  initial_root["signed"].removeMember("roles");
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director, initial_root, root1), Uptane::InvalidMetadata);
}

/**
 * Check that a root.json that uses "method": "rsassa-pss-sha256" validates correctly
 * See PRO-2999
 */
TEST(Root, RootJsonRsassaPssSha256) {
  Uptane::Root root1(Uptane::Root::Policy::kAcceptAll);
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/rsassa-pss-sha256/root.json");
  LOG_INFO << "Root is:" << initial_root;

  Uptane::Root root(Uptane::RepositoryType::Director, initial_root, root1);
  EXPECT_NO_THROW(Uptane::Root(Uptane::RepositoryType::Director, initial_root, root));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
