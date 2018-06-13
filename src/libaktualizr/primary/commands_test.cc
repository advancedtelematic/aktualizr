#include <gtest/gtest.h>

#include <string>

#include "picojson.h"

#include "primary/commands.h"

TEST(command, Shutdown_comand_to_json) {
  command::Shutdown comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "Shutdown");
}

TEST(command, FetchMeta_comand_to_json) {
  command::FetchMeta comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "FetchMeta");
}

TEST(command, StartDownload_comand_to_json) {
  Json::Value target_json;
  target_json["custom"]["ecuIdentifier"] = "ecu1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);
  command::StartDownload comand(targets);

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["fields"][0]["test"]["custom"]["ecuIdentifier"], "ecu1");
  EXPECT_EQ(json["fields"][0]["test"]["hashes"]["sha256"], "12AB");
  EXPECT_EQ(json["variant"].asString(), "StartDownload");
}

TEST(command, StartDownload_command_from_pico_json) {
  Json::Value target_json;
  target_json["custom"]["ecuIdentifier"] = "ecu1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);
  command::StartDownload start_download(targets);

  picojson::value val;
  picojson::parse(val, start_download.toJson());
  auto comand = std::static_pointer_cast<command::StartDownload>(command::BaseCommand::fromPicoJson(val));

  EXPECT_EQ(comand->variant, "StartDownload");
  EXPECT_EQ(comand->updates[0].ecu_identifier().ToString(), "ecu1");
  EXPECT_EQ(comand->updates[0].filename(), "test");
  EXPECT_EQ(comand->updates[0].sha256Hash(), "12ab");
}

TEST(command, Shutdown_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"Shutdown\"}";

  picojson::value val;
  picojson::parse(val, json);
  std::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "Shutdown");
}

TEST(command, Nonexistent_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"Nonexistent\"}";

  picojson::value val;
  picojson::parse(val, json);
  try {
    std::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);
  } catch (std::runtime_error e) {
    ASSERT_STREQ(e.what(), "wrong command variant = Nonexistent");
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
