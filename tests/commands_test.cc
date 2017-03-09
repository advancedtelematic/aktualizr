#include <gtest/gtest.h>

#include <string>
#include "picojson.h"

#include "commands.h"

TEST(command, Shutdown_comand_to_json) {
  command::Shutdown comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "Shutdown");
}

TEST(command, GetUpdateRequests_comand_to_json) {
  command::GetUpdateRequests comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "GetUpdateRequests");
}

TEST(command, ListInstalledPackages_comand_to_json) {
  command::ListInstalledPackages comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "ListInstalledPackages");
}

TEST(command, ListSystemInfo_comand_to_json) {
  command::ListSystemInfo comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "ListSystemInfo");
}

TEST(command, StartDownload_comand_to_json) {
  command::StartDownload comand("testid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["fields"][0].asString(), "testid");
  EXPECT_EQ(json["variant"].asString(), "StartDownload");
}

TEST(command, StartDownload_event_from_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"StartDownload\"}";
  command::StartDownload comand = command::StartDownload::fromJson(json);

  EXPECT_EQ(comand.variant, "StartDownload");
  EXPECT_EQ(comand.update_request_id, "testid");
}

TEST(command, AbortDownload_comand_to_json) {
  command::AbortDownload comand("testid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["fields"][0].asString(), "testid");
  EXPECT_EQ(json["variant"].asString(), "AbortDownload");
}

TEST(command, AbortDownload_event_from_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"AbortDownload\"}";
  command::AbortDownload comand = command::AbortDownload::fromJson(json);

  EXPECT_EQ(comand.variant, "AbortDownload");
  EXPECT_EQ(comand.update_request_id, "testid");
}

TEST(command, StartInstall_comand_to_json) {
  command::StartInstall comand("testid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["fields"][0].asString(), "testid");
  EXPECT_EQ(json["variant"].asString(), "StartInstall");
}

TEST(command, InstallFailed_event_from_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"StartInstall\"}";
  command::StartInstall comand = command::StartInstall::fromJson(json);

  EXPECT_EQ(comand.variant, "StartInstall");
  EXPECT_EQ(comand.update_request_id, "testid");
}

TEST(command, SendInstalledPackages_comand_to_json) {
  data::Package p1, p2;
  p1.name = "packagename1";
  p1.version = "v2.0";

  std::vector<data::Package> packages;
  packages.push_back(p1);

  command::SendInstalledPackages comand(packages);

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "SendInstalledPackages");
  EXPECT_EQ(json["fields"][0][0]["name"].asString(), "packagename1");
  EXPECT_EQ(json["fields"][0][0]["version"].asString(), "v2.0");
}

TEST(command, SendInstalledPackages_comand_from_json) {
  std::string json =
      "{\"fields\":[[{\"name\":\"packagename1\",\"version\":\"v2.0\"},{"
      "\"name\":\"packagename2\",\"version\":\"v3.0\"}]],\"variant\":"
      "\"FoundInstalledPackages\"}";
  command::SendInstalledPackages comand = command::SendInstalledPackages::fromJson(json);

  EXPECT_EQ(comand.variant, "SendInstalledPackages");
  EXPECT_EQ(comand.packages[0].name, "packagename1");
  EXPECT_EQ(comand.packages[0].version, "v2.0");
  EXPECT_EQ(comand.packages[1].name, "packagename2");
  EXPECT_EQ(comand.packages[1].version, "v3.0");
}

TEST(command, SendSystemInfo_comand_to_json) {
  command::SendSystemInfo comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "SendSystemInfo");
}

TEST(command, SendUpdateReport_comand_to_json) {
  data::OperationResult operation_result;
  operation_result.id = "testid23";
  operation_result.result_code = data::NOT_FOUND;
  operation_result.result_text = "text";
  std::vector<data::OperationResult> operation_results;
  operation_results.push_back(operation_result);
  data::UpdateReport ur;
  ur.update_id = "request_id";
  ur.operation_results = operation_results;

  command::SendUpdateReport comand(ur);

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "SendUpdateReport");
  EXPECT_EQ(json["fields"][0]["update_id"].asString(), "request_id");
  EXPECT_EQ(json["fields"][0]["operation_results"][0]["id"].asString(), "testid23");
  EXPECT_EQ(json["fields"][0]["operation_results"][0]["result_code"].asUInt(), data::NOT_FOUND);
  EXPECT_EQ(json["fields"][0]["operation_results"][0]["result_text"].asString(), "text");
}

TEST(command, SendUpdateReport_comand_from_json) {
  std::string json =
      "{\"fields\":[{\"operation_results\":[{\"id\":\"testid23\",\"result_"
      "code\":16,\"result_text\":\"text\"}],\"update_id\":\"request_id\"}],"
      "\"variant\":\"SendUpdateReport\"}";
  command::SendUpdateReport comand = command::SendUpdateReport::fromJson(json);

  EXPECT_EQ(comand.variant, "SendUpdateReport");
  EXPECT_EQ(comand.update_report.operation_results[0].id, "testid23");
  EXPECT_EQ(comand.update_report.operation_results[0].result_code, 16);
  EXPECT_EQ(comand.update_report.operation_results[0].result_text, "text");
  EXPECT_EQ(comand.update_report.update_id, "request_id");
}

TEST(command, SendInstalledSoftware_comand_to_json) {
  data::InstalledPackage package;
  package.package_id = "id";
  package.name = "testname";
  package.description = "testdescription";
  package.last_modified = 54321;
  std::vector<data::InstalledPackage> packages;
  packages.push_back(package);

  data::InstalledFirmware firmware;
  firmware.module = "testmodule";
  firmware.firmware_id = "firmware_id123";
  firmware.last_modified = 12345;

  std::vector<data::InstalledFirmware> firmwares;
  firmwares.push_back(firmware);

  data::InstalledSoftware installed_software;
  installed_software.firmwares = firmwares;
  installed_software.packages = packages;

  command::SendInstalledSoftware comand(installed_software);

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "SendInstalledSoftware");
  EXPECT_EQ(json["fields"][0]["packages"][0]["package_id"].asString(), "id");
  EXPECT_EQ(json["fields"][0]["packages"][0]["name"].asString(), "testname");
  EXPECT_EQ(json["fields"][0]["packages"][0]["description"].asString(), "testdescription");
  EXPECT_EQ(json["fields"][0]["packages"][0]["last_modified"].asUInt(), 54321);

  EXPECT_EQ(json["fields"][0]["firmwares"][0]["module"].asString(), "testmodule");
  EXPECT_EQ(json["fields"][0]["firmwares"][0]["firmware_id"].asString(), "firmware_id123");
  EXPECT_EQ(json["fields"][0]["firmwares"][0]["last_modified"].asUInt(), 12345);
}

TEST(command, SendInstalledSoftware_comand_from_json) {
  std::string json =
      "{\"fields\" : [{\"firmwares\" : [ {\"firmware_id\" : "
      "\"firmware_id123\", \"last_modified\" : 12345, \"module\" : "
      "\"testmodule\"}], \"packages\" :  [ { \"description\" : "
      "\"testdescription\",  \"last_modified\" : 54321, \"name\" : "
      "\"testname\",\"package_id\" : \"id\"}]}],\"variant\" : "
      "\"SendInstalledSoftware\"}";
  command::SendInstalledSoftware comand = command::SendInstalledSoftware::fromJson(json);

  EXPECT_EQ(comand.variant, "SendInstalledSoftware");
  EXPECT_EQ(comand.installed_software.firmwares[0].module, "testmodule");
  EXPECT_EQ(comand.installed_software.firmwares[0].firmware_id, "firmware_id123");
  EXPECT_EQ(comand.installed_software.firmwares[0].last_modified, 12345);

  EXPECT_EQ(comand.installed_software.packages[0].package_id, "id");
  EXPECT_EQ(comand.installed_software.packages[0].name, "testname");

  EXPECT_EQ(comand.installed_software.packages[0].description, "testdescription");
  EXPECT_EQ(comand.installed_software.packages[0].last_modified, 54321);
}

TEST(command, Authenticate_comand_to_json) {
  command::Authenticate comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "Authenticate");
}

TEST(command, Authenticate_command_from_pico_json) {
  std::string json =
      "{\"fields\" : [{\"client_id\" : \"client123\", \"client_secret\" :  "
      "\"secret\"}], \"variant\" : \"Authenticate\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "Authenticate");
}

TEST(command, SendInstalledSoftware_command_from_pico_json) {
  std::string json =
      "{\"fields\" : [{\"firmwares\" : [ {\"firmware_id\" : "
      "\"firmware_id123\", \"last_modified\" : 12345, \"module\" : "
      "\"testmodule\"}], \"packages\" :  [ { \"description\" : "
      "\"testdescription\",  \"last_modified\" : 54321, \"name\" : "
      "\"testname\",\"package_id\" : \"id\"}]}],\"variant\" : "
      "\"SendInstalledSoftware\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "SendInstalledSoftware");
}

TEST(command, SendUpdateReport_command_from_pico_json) {
  std::string json =
      "{\"fields\":[{\"operation_results\":[{\"id\":\"testid23\",\"result_"
      "code\":16,\"result_text\":\"text\"}],\"update_id\":\"request_id\"}],"
      "\"variant\":\"SendUpdateReport\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "SendUpdateReport");
}

TEST(command, SendInstalledPackages_command_from_pico_json) {
  std::string json =
      "{\"fields\":[[{\"name\":\"packagename1\",\"version\":\"v2.0\"},{"
      "\"name\":\"packagename2\",\"version\":\"v3.0\"}]],\"variant\":"
      "\"SendInstalledPackages\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "SendInstalledPackages");
}

TEST(command, AbortDownload_command_from_pico_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"AbortDownload\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "AbortDownload");
}

TEST(command, StartDownload_command_from_pico_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"StartDownload\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "StartDownload");
}

TEST(command, Shutdown_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"Shutdown\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "Shutdown");
}

TEST(command, ListInstalledPackages_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"ListInstalledPackages\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "ListInstalledPackages");
}

TEST(command, ListSystemInfo_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"ListSystemInfo\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "ListSystemInfo");
}

TEST(command, StartInstall_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"StartInstall\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "StartInstall");
}

TEST(command, SendSystemInfo_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"SendSystemInfo\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "SendSystemInfo");
}

TEST(command, Nonexistent_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"Nonexistent\"}";

  picojson::value val;
  picojson::parse(val, json);
  try {
    boost::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);
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