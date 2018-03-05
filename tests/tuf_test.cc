#include <gtest/gtest.h>

#include <json/json.h>

#include "logging.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"
#include "utils.h"

Uptane::TimeStamp now("2017-01-01T01:00:00Z");

TEST(Root, RootValidates) {
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/sample1/root.json");
  LOG_INFO << "Root is:" << initial_root;

  Uptane::Root root1(Uptane::Root::kAcceptAll);
  Json::Value i = root1.UnpackSignedObject(now, "director", Uptane::Role::Root(), initial_root);
  Uptane::Root root("director", i);

  Json::Value unpacked = root.UnpackSignedObject(now, "director", Uptane::Role::Root(), initial_root);
  EXPECT_EQ(unpacked, initial_root);
}

TEST(Root, RootJsonNoKeys) {
  Uptane::Root root1(Uptane::Root::kAcceptAll);
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/sample1/root.json");
  LOG_INFO << "Root is:" << initial_root;
  Json::Value i = root1.UnpackSignedObject(now, "director", Uptane::Role::Root(), initial_root);
  i["signed"].removeMember("keys");
  EXPECT_THROW(Uptane::Root("director", i), Uptane::InvalidMetadata);
}

TEST(Root, RootJsonNoRoles) {
  Uptane::Root root1(Uptane::Root::kAcceptAll);
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/sample1/root.json");
  LOG_INFO << "Root is:" << initial_root;
  Json::Value i = root1.UnpackSignedObject(now, "director", Uptane::Role::Root(), initial_root);
  i["signed"].removeMember("roles");
  EXPECT_THROW(Uptane::Root("director", i), Uptane::InvalidMetadata);
}

/**
 * Check that a root.json that uses "method": "rsassa-pss-sha256" validates correctly
 * See PRO-2999
 */
TEST(Root, RootJsonRsassaPssSha256) {
  Uptane::Root root1(Uptane::Root::kAcceptAll);
  Json::Value initial_root = Utils::parseJSONFile("tests/tuf/rsassa-pss-sha256/root.json");
  LOG_INFO << "Root is:" << initial_root;
  Json::Value i = root1.UnpackSignedObject(now, "director", Uptane::Role::Root(), initial_root);
  try {
    Uptane::Root root("director", i);
    root.UnpackSignedObject(now, "director", Uptane::Role::Root(), initial_root);
    EXPECT_TRUE(1);
  } catch (Uptane::InvalidMetadata& err) {
    EXPECT_STREQ("", err.what());
    FAIL();
  }
}

TEST(TimeStamp, Parsing) {
  Uptane::TimeStamp t_old("2038-01-19T02:00:00Z");
  Uptane::TimeStamp t_new("2038-01-19T03:14:06Z");

  Uptane::TimeStamp t_invalid;

  EXPECT_LT(t_old, t_new);
  EXPECT_GT(t_new, t_old);
  EXPECT_FALSE(t_invalid < t_old);
  EXPECT_FALSE(t_old < t_invalid);
  EXPECT_FALSE(t_invalid < t_invalid);
}

TEST(TimeStamp, ParsingInvalid) { EXPECT_THROW(Uptane::TimeStamp("2038-01-19T0"), Uptane::InvalidMetadata); }

TEST(TimeStamp, Now) {
  Uptane::TimeStamp t_past("1982-12-13T02:00:00Z");
  Uptane::TimeStamp t_future("2038-01-19T03:14:06Z");
  Uptane::TimeStamp t_now(Uptane::TimeStamp::Now());

  EXPECT_LT(t_past, t_now);
  EXPECT_LT(t_now, t_future);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
