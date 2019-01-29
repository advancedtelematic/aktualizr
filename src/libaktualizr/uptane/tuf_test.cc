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

/* Validate TUF roles. */
TEST(Role, ValidateRoles) {
  Uptane::Role root = Uptane::Role::Root();
  EXPECT_EQ(root.ToInt(), 0);
  EXPECT_EQ(root.ToString(), "root");
  EXPECT_EQ(root.IsDelegation(), false);

  Uptane::Role snapshot = Uptane::Role::Snapshot();
  EXPECT_EQ(snapshot.ToInt(), 1);
  EXPECT_EQ(snapshot.ToString(), "snapshot");
  EXPECT_EQ(snapshot.IsDelegation(), false);

  Uptane::Role targets = Uptane::Role::Targets();
  EXPECT_EQ(targets.ToInt(), 2);
  EXPECT_EQ(targets.ToString(), "targets");
  EXPECT_EQ(targets.IsDelegation(), false);

  Uptane::Role timestamp = Uptane::Role::Timestamp();
  EXPECT_EQ(timestamp.ToInt(), 3);
  EXPECT_EQ(timestamp.ToString(), "timestamp");
  EXPECT_EQ(timestamp.IsDelegation(), false);
}

/* Delegated roles have custom names. */
TEST(Role, ValidateDelegation) {
  Uptane::Role delegated = Uptane::Role::Delegation("whatever");
  EXPECT_EQ(delegated.ToString(), "whatever");
  EXPECT_EQ(delegated.IsDelegation(), true);
}

/* Reject delegated role names that are identical to reserved role names. */
TEST(Role, InvalidDelegationName) {
  EXPECT_THROW(Uptane::Role::Delegation("root"), Uptane::Exception);
  EXPECT_THROW(Uptane::Role::Delegation("snapshot"), Uptane::Exception);
  EXPECT_THROW(Uptane::Role::Delegation("targets"), Uptane::Exception);
  EXPECT_THROW(Uptane::Role::Delegation("timestamp"), Uptane::Exception);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
