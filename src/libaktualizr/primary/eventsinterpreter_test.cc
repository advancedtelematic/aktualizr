#include <gtest/gtest.h>

#include <string>

#include "config/config.h"
#include "primary/commands.h"
#include "primary/events.h"
#include "primary/eventsinterpreter.h"

TEST(event, RunningMode_full) {
  Config conf;
  conf.uptane.running_mode = RunningMode::kFull;
  conf.uptane.polling_sec = 1;
  std::shared_ptr<command::Channel> commands_channel{new command::Channel};
  std::shared_ptr<event::Channel> events_channel{new event::Channel};

  EventsInterpreter intertpreter(conf, events_channel, commands_channel);
  intertpreter.interpret();
  std::shared_ptr<command::BaseCommand> command;

  *commands_channel >> command;
  EXPECT_EQ(command->variant, "SendDeviceData");
  *events_channel << std::make_shared<event::SendDeviceDataComplete>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "FetchMeta");
  *events_channel << std::make_shared<event::FetchMetaComplete>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "CheckUpdates");

  Json::Value target_json;
  target_json["custom"]["ecuIdentifier"] = "ecu1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);

  *events_channel << std::make_shared<event::UpdateAvailable>(targets);
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "StartDownload");
  *events_channel << std::make_shared<event::DownloadComplete>(targets);
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "UptaneInstall");
  *events_channel << std::make_shared<event::InstallComplete>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "FetchMeta");

  // Try again but without updates now
  *events_channel << std::make_shared<event::FetchMetaComplete>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "CheckUpdates");
  *events_channel << std::make_shared<event::UptaneTimestampUpdated>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "FetchMeta");
}

TEST(event, RunningMode_once) {
  Config conf;
  conf.uptane.running_mode = RunningMode::kOnce;
  conf.uptane.polling_sec = 1;
  std::shared_ptr<command::Channel> commands_channel{new command::Channel};
  std::shared_ptr<event::Channel> events_channel{new event::Channel};

  EventsInterpreter intertpreter(conf, events_channel, commands_channel);
  intertpreter.interpret();
  std::shared_ptr<command::BaseCommand> command;

  *commands_channel >> command;
  EXPECT_EQ(command->variant, "SendDeviceData");
  *events_channel << std::make_shared<event::SendDeviceDataComplete>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "FetchMeta");
  *events_channel << std::make_shared<event::FetchMetaComplete>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "CheckUpdates");

  Json::Value target_json;
  target_json["custom"]["ecuIdentifier"] = "ecu1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);

  *events_channel << std::make_shared<event::UpdateAvailable>(targets);
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "StartDownload");
  *events_channel << std::make_shared<event::DownloadComplete>(targets);
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "UptaneInstall");
  *events_channel << std::make_shared<event::InstallComplete>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "Shutdown");
}

TEST(event, RunningMode_check) {
  Config conf;
  conf.uptane.running_mode = RunningMode::kCheck;
  conf.uptane.polling_sec = 1;
  std::shared_ptr<command::Channel> commands_channel{new command::Channel};
  std::shared_ptr<event::Channel> events_channel{new event::Channel};

  EventsInterpreter intertpreter(conf, events_channel, commands_channel);
  intertpreter.interpret();
  std::shared_ptr<command::BaseCommand> command;

  *commands_channel >> command;
  EXPECT_EQ(command->variant, "FetchMeta");
  *events_channel << std::make_shared<event::FetchMetaComplete>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "CheckUpdates");

  Json::Value target_json;
  target_json["custom"]["ecuIdentifier"] = "ecu1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);

  *events_channel << std::make_shared<event::UpdateAvailable>(targets);
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "Shutdown");
}

TEST(event, RunningMode_download) {
  Config conf;
  conf.uptane.running_mode = RunningMode::kDownload;
  conf.uptane.polling_sec = 1;
  std::shared_ptr<command::Channel> commands_channel{new command::Channel};
  std::shared_ptr<event::Channel> events_channel{new event::Channel};

  EventsInterpreter intertpreter(conf, events_channel, commands_channel);
  intertpreter.interpret();
  std::shared_ptr<command::BaseCommand> command;

  *commands_channel >> command;
  EXPECT_EQ(command->variant, "CheckUpdates");

  Json::Value target_json;
  target_json["custom"]["ecuIdentifier"] = "ecu1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);

  *events_channel << std::make_shared<event::UpdateAvailable>(targets);
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "StartDownload");
  *events_channel << std::make_shared<event::DownloadComplete>(targets);
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "Shutdown");
}

TEST(event, RunningMode_install) {
  Config conf;
  conf.uptane.running_mode = RunningMode::kInstall;
  conf.uptane.polling_sec = 1;
  std::shared_ptr<command::Channel> commands_channel{new command::Channel};
  std::shared_ptr<event::Channel> events_channel{new event::Channel};

  EventsInterpreter intertpreter(conf, events_channel, commands_channel);
  intertpreter.interpret();
  std::shared_ptr<command::BaseCommand> command;

  *commands_channel >> command;
  EXPECT_EQ(command->variant, "CheckUpdates");

  Json::Value target_json;
  target_json["custom"]["ecuIdentifier"] = "ecu1";
  target_json["hashes"]["sha256"] = "12AB";
  target_json["length"] = 1;
  Uptane::Target target("test", target_json);
  std::vector<Uptane::Target> targets;
  targets.push_back(target);

  *events_channel << std::make_shared<event::UpdateAvailable>(targets);
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "UptaneInstall");
  *events_channel << std::make_shared<event::InstallComplete>();
  *commands_channel >> command;
  EXPECT_EQ(command->variant, "Shutdown");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
