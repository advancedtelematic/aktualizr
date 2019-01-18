#include <gtest/gtest.h>

#include <json/json.h>

#include "logging/logging.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"
#include "utilities/utils.h"

/* Validate a TUF root. */
TEST(Root, RootValidates) {
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/sample1/root.json");
  LOG_INFO << "Root is:" << initial_root;

  Uptane::Root root1(Uptane::Root::Policy::kAcceptAll);
  Uptane::Root root(Uptane::RepositoryType::Director(), initial_root, root1);

  EXPECT_NO_THROW(Uptane::Root(Uptane::RepositoryType::Director(), initial_root, root));
}

/* Throw an exception if a TUF root is unsigned. */
TEST(Root, RootJsonNoKeys) {
  Uptane::Root root1(Uptane::Root::Policy::kAcceptAll);
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/sample1/root.json");
  initial_root["signed"].removeMember("keys");
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director(), initial_root, root1), Uptane::InvalidMetadata);
}

/* Throw an exception if a TUF root has no roles. */
TEST(Root, RootJsonNoRoles) {
  Uptane::Root root1(Uptane::Root::Policy::kAcceptAll);
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/sample1/root.json");
  initial_root["signed"].removeMember("roles");
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director(), initial_root, root1), Uptane::InvalidMetadata);
}

/**
 * Check that a root.json that uses "method": "rsassa-pss-sha256" validates correctly
 * See PRO-2999
 */
TEST(Root, RootJsonRsassaPssSha256) {
  Uptane::Root root1(Uptane::Root::Policy::kAcceptAll);
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/rsassa-pss-sha256/root.json");
  LOG_INFO << "Root is:" << initial_root;

  Uptane::Root root(Uptane::RepositoryType::Director(), initial_root, root1);
  EXPECT_NO_THROW(Uptane::Root(Uptane::RepositoryType::Director(), initial_root, root));
}

/* Reject delegated role names that are identical to other roles. */
TEST(Role, InvalidDelegationName) {
  EXPECT_THROW(Uptane::Role::Delegated("root"), Uptane::Exception);
  EXPECT_THROW(Uptane::Role::Delegated("snapshot"), Uptane::Exception);
  EXPECT_THROW(Uptane::Role::Delegated("targets"), Uptane::Exception);
  EXPECT_THROW(Uptane::Role::Delegated("timestamp"), Uptane::Exception);
}

/* Delegated role has custom name. */
TEST(Role, ValidDelegationName) {
  Uptane::Role delegated = Uptane::Role::Delegated("whatever");
  EXPECT_EQ(delegated.ToString(), "whatever");
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
