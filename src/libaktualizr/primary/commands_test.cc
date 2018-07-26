#include <gtest/gtest.h>

#include <string>

#include <json/json.h>

#include "primary/commands.h"

TEST(command, Shutdown_command_to_json) {
  command::Shutdown command;

  Json::Value json = command.toJson();

  EXPECT_EQ(json["variant"].asString(), "Shutdown");
}

TEST(command, FetchMeta_command_to_json) {
  command::FetchMeta command;

  Json::Value json = command.toJson();

  EXPECT_EQ(json["variant"].asString(), "FetchMeta");
}

TEST(command, StartDownload_command_to_and_from_json) {
  Json::Value target_json;
  target_json["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"] = "hw1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);
  command::StartDownload start_download(targets);

  Json::Value val = start_download.toJson();

  EXPECT_EQ(val["fields"][0]["test"]["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"], "hw1");
  EXPECT_EQ(val["fields"][0]["test"]["hashes"]["sha256"], "12AB");
  EXPECT_EQ(val["variant"].asString(), "StartDownload");

  auto command = std::static_pointer_cast<command::StartDownload>(command::BaseCommand::fromJson(val));

  EXPECT_EQ(command->variant, "StartDownload");
  EXPECT_TRUE(command->updates[0].ecus().find(Uptane::EcuSerial("ecu1")) != command->updates[0].ecus().end());
  EXPECT_EQ(command->updates[0].filename(), "test");
  EXPECT_EQ(command->updates[0].sha256Hash(), "12ab");
}

TEST(command, UptaneInstall_command_to_and_from_json) {
  Json::Value target_json;
  target_json["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"] = "hw1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);
  command::UptaneInstall uptane_install(targets);

  Json::Value val = uptane_install.toJson();

  EXPECT_EQ(val["fields"][0]["test"]["custom"]["ecuIdentifiers"]["ecu1"]["hardwareId"], "hw1");
  EXPECT_EQ(val["fields"][0]["test"]["hashes"]["sha256"], "12AB");
  EXPECT_EQ(val["variant"].asString(), "UptaneInstall");

  auto command = std::static_pointer_cast<command::UptaneInstall>(command::BaseCommand::fromJson(val));

  EXPECT_EQ(command->variant, "UptaneInstall");
  EXPECT_TRUE(command->packages[0].ecus().find(Uptane::EcuSerial("ecu1")) != command->packages[0].ecus().end());
  EXPECT_EQ(command->packages[0].filename(), "test");
  EXPECT_EQ(command->packages[0].sha256Hash(), "12ab");
}

TEST(command, Shutdown_command_from_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"Shutdown\"}";

  Json::Value val;
  Json::Reader reader;
  reader.parse(json, val);
  std::shared_ptr<command::BaseCommand> command = command::BaseCommand::fromJson(val);

  EXPECT_EQ(command->variant, "Shutdown");
}

TEST(command, Nonexistent_command_from_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"Nonexistent\"}";

  Json::Value val;
  Json::Reader reader;
  reader.parse(json, val);
  try {
    std::shared_ptr<command::BaseCommand> command = command::BaseCommand::fromJson(val);
  } catch (const std::runtime_error &e) {
    ASSERT_STREQ(e.what(), "wrong command variant = Nonexistent");
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
