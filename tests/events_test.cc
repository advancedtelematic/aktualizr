#include <gtest/gtest.h>

#include <string>

#include "primary/events.h"

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
  data::UpdateAvailable ua;
  ua.update_id = "id4";
  ua.signature = "sign";
  ua.description = "this is description";
  ua.request_confirmation = true;
  ua.size = 5000;

  event::UpdateAvailable event(ua);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "UpdateAvailable");
  EXPECT_EQ(json["fields"][0]["update_id"].asString(), "id4");
  EXPECT_EQ(json["fields"][0]["signature"].asString(), "sign");
  EXPECT_EQ(json["fields"][0]["description"].asBool(), true);
}

TEST(event, DownloadComplete_event_to_json) {
  data::DownloadComplete dc;
  dc.update_id = "updateid123";
  dc.update_image = "some text";
  dc.signature = "sign";

  event::DownloadComplete event(dc);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "DownloadComplete");
  EXPECT_EQ(json["fields"][0]["update_id"].asString(), "updateid123");
  EXPECT_EQ(json["fields"][0]["update_image"].asString(), "some text");
  EXPECT_EQ(json["fields"][0]["signature"].asString(), "sign");
}

TEST(event, InstalledSoftwareNeeded_event_to_json) {
  event::InstalledSoftwareNeeded event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "InstalledSoftwareNeeded");
}

TEST(event, UptaneTimestampUpdated_event_to_json) {
  event::UptaneTimestampUpdated event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "UptaneTimestampUpdated");
}

TEST(event, UptaneTargetsUpdated_event_to_json) {
  Json::Value target_json;
  target_json["ecu_serial"] = "test1";
  target_json["ref_name"] = "test2";
  target_json["description"] = "test3";
  target_json["pull_uri"] = "test4";
  Uptane::Target package("test_package", target_json);
  std::vector<Uptane::Target> packages;

  packages.push_back(package);
  event::UptaneTargetsUpdated event(packages);
  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "UptaneTargetsUpdated");
}

TEST(event, Error_event_from_json) {
  std::string json = "{\"fields\":[\"Error123\"],\"variant\":\"Error\"}";
  event::Error event = event::Error::fromJson(json);
  EXPECT_EQ(event.variant, "Error");
  EXPECT_EQ(event.message, "Error123");
}

TEST(event, UpdateAvailable_event_from_json) {
  std::string json =
      "{\"fields\":[{\"description\":\"this is "
      "description\",\"request_confirmation\":true,\"signature\":\"sign\","
      "\"size\":5000,\"update_id\":\"id4\"}],\"variant\":\"UpdateAvailable\"}";
  event::UpdateAvailable event = event::UpdateAvailable::fromJson(json);

  EXPECT_EQ(event.variant, "UpdateAvailable");
  EXPECT_EQ(event.update_vailable.description, "this is description");
  EXPECT_EQ(event.update_vailable.request_confirmation, true);
  EXPECT_EQ(event.update_vailable.signature, "sign");
  EXPECT_EQ(event.update_vailable.size, 5000u);
  EXPECT_EQ(event.update_vailable.update_id, "id4");
}

TEST(event, DownloadComplete_event_from_json) {
  std::string json =
      "{\"fields\":[{\"signature\":\"sign\",\"update_id\":\"updateid123\","
      "\"update_image\":\"some text\"}],\"variant\":\"DownloadComplete\"}";
  event::DownloadComplete event = event::DownloadComplete::fromJson(json);

  EXPECT_EQ(event.variant, "DownloadComplete");
  EXPECT_EQ(event.download_complete.signature, "sign");
  EXPECT_EQ(event.download_complete.update_id, "updateid123");
  EXPECT_EQ(event.download_complete.update_image, "some text");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
