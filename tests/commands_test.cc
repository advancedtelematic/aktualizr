#define BOOST_TEST_MODULE test_comads

#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test.hpp>
#include <string>
#include "picojson.h"

#include "commands.h"

BOOST_AUTO_TEST_CASE(Shutdown_comand_to_json) {
  command::Shutdown comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "Shutdown");
}

BOOST_AUTO_TEST_CASE(GetUpdateRequests_comand_to_json) {
  command::GetUpdateRequests comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "GetUpdateRequests");
}

BOOST_AUTO_TEST_CASE(ListInstalledPackages_comand_to_json) {
  command::ListInstalledPackages comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "ListInstalledPackages");
}

BOOST_AUTO_TEST_CASE(ListSystemInfo_comand_to_json) {
  command::ListSystemInfo comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "ListSystemInfo");
}

BOOST_AUTO_TEST_CASE(StartDownload_comand_to_json) {
  command::StartDownload comand("testid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["fields"][0].asString(), "testid");
  BOOST_CHECK_EQUAL(json["variant"].asString(), "StartDownload");
}

BOOST_AUTO_TEST_CASE(StartDownload_event_from_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"StartDownload\"}";
  command::StartDownload comand = command::StartDownload::fromJson(json);

  BOOST_CHECK_EQUAL(comand.variant, "StartDownload");
  BOOST_CHECK_EQUAL(comand.update_request_id, "testid");
}

BOOST_AUTO_TEST_CASE(AbortDownload_comand_to_json) {
  command::AbortDownload comand("testid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["fields"][0].asString(), "testid");
  BOOST_CHECK_EQUAL(json["variant"].asString(), "AbortDownload");
}

BOOST_AUTO_TEST_CASE(AbortDownload_event_from_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"AbortDownload\"}";
  command::AbortDownload comand = command::AbortDownload::fromJson(json);

  BOOST_CHECK_EQUAL(comand.variant, "AbortDownload");
  BOOST_CHECK_EQUAL(comand.update_request_id, "testid");
}

BOOST_AUTO_TEST_CASE(StartInstall_comand_to_json) {
  command::StartInstall comand("testid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["fields"][0].asString(), "testid");
  BOOST_CHECK_EQUAL(json["variant"].asString(), "StartInstall");
}

BOOST_AUTO_TEST_CASE(InstallFailed_event_from_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"StartInstall\"}";
  command::StartInstall comand = command::StartInstall::fromJson(json);

  BOOST_CHECK_EQUAL(comand.variant, "StartInstall");
  BOOST_CHECK_EQUAL(comand.update_request_id, "testid");
}

BOOST_AUTO_TEST_CASE(SendInstalledPackages_comand_to_json) {
  data::Package p1, p2;
  p1.name = "packagename1";
  p1.version = "v2.0";

  std::vector<data::Package> packages;
  packages.push_back(p1);

  command::SendInstalledPackages comand(packages);

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "SendInstalledPackages");
  BOOST_CHECK_EQUAL(json["fields"][0][0]["name"].asString(), "packagename1");
  BOOST_CHECK_EQUAL(json["fields"][0][0]["version"].asString(), "v2.0");
}

BOOST_AUTO_TEST_CASE(SendInstalledPackages_comand_from_json) {
  std::string json =
      "{\"fields\":[[{\"name\":\"packagename1\",\"version\":\"v2.0\"},{"
      "\"name\":\"packagename2\",\"version\":\"v3.0\"}]],\"variant\":"
      "\"FoundInstalledPackages\"}";
  command::SendInstalledPackages comand =
      command::SendInstalledPackages::fromJson(json);

  BOOST_CHECK_EQUAL(comand.variant, "SendInstalledPackages");
  BOOST_CHECK_EQUAL(comand.packages[0].name, "packagename1");
  BOOST_CHECK_EQUAL(comand.packages[0].version, "v2.0");
  BOOST_CHECK_EQUAL(comand.packages[1].name, "packagename2");
  BOOST_CHECK_EQUAL(comand.packages[1].version, "v3.0");
}

BOOST_AUTO_TEST_CASE(SendSystemInfo_comand_to_json) {
  command::SendSystemInfo comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "SendSystemInfo");
}

BOOST_AUTO_TEST_CASE(SendUpdateReport_comand_to_json) {
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

  BOOST_CHECK_EQUAL(json["variant"].asString(), "SendUpdateReport");
  BOOST_CHECK_EQUAL(json["fields"][0]["update_id"].asString(), "request_id");
  BOOST_CHECK_EQUAL(json["fields"][0]["operation_results"][0]["id"].asString(),
                    "testid23");
  BOOST_CHECK_EQUAL(
      json["fields"][0]["operation_results"][0]["result_code"].asUInt(),
      data::NOT_FOUND);
  BOOST_CHECK_EQUAL(
      json["fields"][0]["operation_results"][0]["result_text"].asString(),
      "text");
}

BOOST_AUTO_TEST_CASE(SendUpdateReport_comand_from_json) {
  std::string json =
      "{\"fields\":[{\"operation_results\":[{\"id\":\"testid23\",\"result_"
      "code\":16,\"result_text\":\"text\"}],\"update_id\":\"request_id\"}],"
      "\"variant\":\"SendUpdateReport\"}";
  command::SendUpdateReport comand = command::SendUpdateReport::fromJson(json);

  BOOST_CHECK_EQUAL(comand.variant, "SendUpdateReport");
  BOOST_CHECK_EQUAL(comand.update_report.operation_results[0].id, "testid23");
  BOOST_CHECK_EQUAL(comand.update_report.operation_results[0].result_code, 16);
  BOOST_CHECK_EQUAL(comand.update_report.operation_results[0].result_text,
                    "text");
  BOOST_CHECK_EQUAL(comand.update_report.update_id, "request_id");
}

BOOST_AUTO_TEST_CASE(SendInstalledSoftware_comand_to_json) {
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

  BOOST_CHECK_EQUAL(json["variant"].asString(), "SendInstalledSoftware");
  BOOST_CHECK_EQUAL(json["fields"][0]["packages"][0]["package_id"].asString(),
                    "id");
  BOOST_CHECK_EQUAL(json["fields"][0]["packages"][0]["name"].asString(),
                    "testname");
  BOOST_CHECK_EQUAL(json["fields"][0]["packages"][0]["description"].asString(),
                    "testdescription");
  BOOST_CHECK_EQUAL(json["fields"][0]["packages"][0]["last_modified"].asUInt(),
                    54321);

  BOOST_CHECK_EQUAL(json["fields"][0]["firmwares"][0]["module"].asString(),
                    "testmodule");
  BOOST_CHECK_EQUAL(json["fields"][0]["firmwares"][0]["firmware_id"].asString(),
                    "firmware_id123");
  BOOST_CHECK_EQUAL(json["fields"][0]["firmwares"][0]["last_modified"].asUInt(),
                    12345);
}

BOOST_AUTO_TEST_CASE(SendInstalledSoftware_comand_from_json) {
  std::string json =
      "{\"fields\" : [{\"firmwares\" : [ {\"firmware_id\" : "
      "\"firmware_id123\", \"last_modified\" : 12345, \"module\" : "
      "\"testmodule\"}], \"packages\" :  [ { \"description\" : "
      "\"testdescription\",  \"last_modified\" : 54321, \"name\" : "
      "\"testname\",\"package_id\" : \"id\"}]}],\"variant\" : "
      "\"SendInstalledSoftware\"}";
  command::SendInstalledSoftware comand =
      command::SendInstalledSoftware::fromJson(json);

  BOOST_CHECK_EQUAL(comand.variant, "SendInstalledSoftware");
  BOOST_CHECK_EQUAL(comand.installed_software.firmwares[0].module,
                    "testmodule");
  BOOST_CHECK_EQUAL(comand.installed_software.firmwares[0].firmware_id,
                    "firmware_id123");
  BOOST_CHECK_EQUAL(comand.installed_software.firmwares[0].last_modified,
                    12345);

  BOOST_CHECK_EQUAL(comand.installed_software.packages[0].package_id, "id");
  BOOST_CHECK_EQUAL(comand.installed_software.packages[0].name, "testname");

  BOOST_CHECK_EQUAL(comand.installed_software.packages[0].description,
                    "testdescription");
  BOOST_CHECK_EQUAL(comand.installed_software.packages[0].last_modified, 54321);
}

BOOST_AUTO_TEST_CASE(Authenticate_comand_to_json) {
  data::ClientCredentials creds;
  creds.client_id = "client123";
  creds.client_secret = "secret";

  command::Authenticate comand(creds);

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "Authenticate");
  BOOST_CHECK_EQUAL(json["fields"][0]["client_id"].asString(), "client123");
  BOOST_CHECK_EQUAL(json["fields"][0]["client_secret"].asString(), "secret");
}

BOOST_AUTO_TEST_CASE(Authenticate_comand_from_json) {
  std::string json =
      "{\"fields\" : [{\"client_id\" : \"client123\", \"client_secret\" :  "
      "\"secret\"}], \"variant\" : \"Authenticate\"}";
  command::Authenticate comand = command::Authenticate::fromJson(json);

  BOOST_CHECK_EQUAL(comand.variant, "Authenticate");
  BOOST_CHECK_EQUAL(comand.client_credentials.client_id, "client123");
  BOOST_CHECK_EQUAL(comand.client_credentials.client_secret, "secret");
}

BOOST_AUTO_TEST_CASE(Authenticate_command_from_pico_json) {
  std::string json =
      "{\"fields\" : [{\"client_id\" : \"client123\", \"client_secret\" :  "
      "\"secret\"}], \"variant\" : \"Authenticate\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "Authenticate");
}

BOOST_AUTO_TEST_CASE(SendInstalledSoftware_command_from_pico_json) {
  std::string json =
      "{\"fields\" : [{\"firmwares\" : [ {\"firmware_id\" : "
      "\"firmware_id123\", \"last_modified\" : 12345, \"module\" : "
      "\"testmodule\"}], \"packages\" :  [ { \"description\" : "
      "\"testdescription\",  \"last_modified\" : 54321, \"name\" : "
      "\"testname\",\"package_id\" : \"id\"}]}],\"variant\" : "
      "\"SendInstalledSoftware\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "SendInstalledSoftware");
}

BOOST_AUTO_TEST_CASE(SendUpdateReport_command_from_pico_json) {
  std::string json =
      "{\"fields\":[{\"operation_results\":[{\"id\":\"testid23\",\"result_"
      "code\":16,\"result_text\":\"text\"}],\"update_id\":\"request_id\"}],"
      "\"variant\":\"SendUpdateReport\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "SendUpdateReport");
}

BOOST_AUTO_TEST_CASE(SendInstalledPackages_command_from_pico_json) {
  std::string json =
      "{\"fields\":[[{\"name\":\"packagename1\",\"version\":\"v2.0\"},{"
      "\"name\":\"packagename2\",\"version\":\"v3.0\"}]],\"variant\":"
      "\"SendInstalledPackages\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "SendInstalledPackages");
}

BOOST_AUTO_TEST_CASE(AbortDownload_command_from_pico_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"AbortDownload\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "AbortDownload");
}

BOOST_AUTO_TEST_CASE(StartDownload_command_from_pico_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"StartDownload\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "StartDownload");
}

BOOST_AUTO_TEST_CASE(Shutdown_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"Shutdown\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "Shutdown");
}

BOOST_AUTO_TEST_CASE(ListInstalledPackages_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"ListInstalledPackages\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "ListInstalledPackages");
}

BOOST_AUTO_TEST_CASE(ListSystemInfo_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"ListSystemInfo\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "ListSystemInfo");
}

BOOST_AUTO_TEST_CASE(StartInstall_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"StartInstall\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "StartInstall");
}

BOOST_AUTO_TEST_CASE(SendSystemInfo_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"SendSystemInfo\"}";

  picojson::value val;
  picojson::parse(val, json);
  boost::shared_ptr<command::BaseCommand> comand =
      command::BaseCommand::fromPicoJson(val);

  BOOST_CHECK_EQUAL(comand->variant, "SendSystemInfo");
}

BOOST_AUTO_TEST_CASE(Nonexistent_command_from_pico_json) {
  std::string json =
      "{\"fields\":[],"
      "\"variant\":\"Nonexistent\"}";

  picojson::value val;
  picojson::parse(val, json);
  try {
    boost::shared_ptr<command::BaseCommand> comand =
        command::BaseCommand::fromPicoJson(val);
  } catch (std::runtime_error e) {
    BOOST_CHECK_EQUAL(e.what(), "wrong command variant = Nonexistent");
  }
}