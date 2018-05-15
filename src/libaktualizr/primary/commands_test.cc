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

TEST(command, GetUpdateRequests_comand_to_json) {
  command::GetUpdateRequests comand;

  Json::Reader reader;
  Json::Value json;
  reader.parse(comand.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "GetUpdateRequests");
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

TEST(command, SendUpdateReport_command_from_pico_json) {
  std::string json =
      "{\"fields\":[{\"operation_results\":[{\"id\":\"testid23\",\"result_"
      "code\":16,\"result_text\":\"text\"}],\"update_id\":\"request_id\"}],"
      "\"variant\":\"SendUpdateReport\"}";

  picojson::value val;
  picojson::parse(val, json);
  std::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "SendUpdateReport");
}

TEST(command, AbortDownload_command_from_pico_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"AbortDownload\"}";

  picojson::value val;
  picojson::parse(val, json);
  std::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "AbortDownload");
}

TEST(command, StartDownload_command_from_pico_json) {
  std::string json =
      "{\"fields\":[\"testid\"],"
      "\"variant\":\"StartDownload\"}";

  picojson::value val;
  picojson::parse(val, json);
  std::shared_ptr<command::BaseCommand> comand = command::BaseCommand::fromPicoJson(val);

  EXPECT_EQ(comand->variant, "StartDownload");
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
