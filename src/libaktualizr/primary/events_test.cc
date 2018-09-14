#include <gtest/gtest.h>

#include <string>

#include "utilities/events.h"

TEST(event, Error_event_to_json) {
  std::string error = "Error123";
  event::Error event(error);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "Error");
  EXPECT_EQ(json["fields"][0].asString(), error);
}

TEST(event, UpdateAvailable_event_to_json) {
  Json::Value target_json;
  target_json["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"] = "hw1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);
  event::UpdateAvailable event(targets, 1);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["fields"][0]["test"]["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"], "hw1");
  EXPECT_EQ(json["fields"][0]["test"]["hashes"]["sha256"], "12AB");
  EXPECT_EQ(json["variant"].asString(), "UpdateAvailable");
}

TEST(event, DownloadComplete_event_to_json) {
  Json::Value target_json;
  target_json["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"] = "hw1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);
  event::DownloadComplete event(targets);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["fields"][0]["test"]["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"], "hw1");
  EXPECT_EQ(json["fields"][0]["test"]["hashes"]["sha256"], "12AB");
  EXPECT_EQ(json["variant"].asString(), "DownloadComplete");
}

TEST(event, InstallComplete_event_to_json) {
  event::InstallComplete event(Uptane::EcuSerial("123456"), true);
  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "InstallComplete");
}

TEST(event, Error_event_from_json) {
  std::string json = "{\"fields\":[\"Error123\"],\"variant\":\"Error\"}";
  event::Error event = event::Error::fromJson(json);
  EXPECT_EQ(event.variant, "Error");
  EXPECT_EQ(event.message, "Error123");
}

TEST(event, UpdateAvailable_event_from_json) {
  Json::Value target_json;
  target_json["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"] = "hw1";
  target_json["hashes"]["sha256"] = "12ab";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);
  event::UpdateAvailable update_available(targets, 1);
  event::UpdateAvailable event = event::UpdateAvailable::fromJson(update_available.toJson());

  EXPECT_EQ(event.variant, "UpdateAvailable");
  EXPECT_TRUE(event.updates[0].ecus().find(Uptane::EcuSerial("ecu1")) != event.updates[0].ecus().end());
  EXPECT_EQ(event.updates[0].filename(), "test");
  EXPECT_EQ(event.updates[0].sha256Hash(), "12ab");
}

TEST(event, DownloadComplete_event_from_json) {
  Json::Value target_json;
  target_json["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"] = "hw1";
  target_json["hashes"]["sha256"] = "12ab";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);
  event::DownloadComplete update_available(targets);
  event::DownloadComplete event = event::DownloadComplete::fromJson(update_available.toJson());

  EXPECT_EQ(event.variant, "DownloadComplete");
  EXPECT_TRUE(event.updates[0].ecus().find(Uptane::EcuSerial("ecu1")) != event.updates[0].ecus().end());
  EXPECT_EQ(event.updates[0].filename(), "test");
  EXPECT_EQ(event.updates[0].sha256Hash(), "12ab");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
