#include <gtest/gtest.h>
#include <string>

#include "events.h"

#ifdef BUILD_OSTREE
#include "ostree.h"
#endif

TEST(event, Error_event_to_json) {
  std::string error = "Error123";
  event::Error event(error);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "Error");
  EXPECT_EQ(json["fields"][0].asString(), error);
}

TEST(event, Authenticated_event_to_json) {
  event::Authenticated event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "Authenticated");
}

TEST(event, NotAuthenticated_event_to_json) {
  event::NotAuthenticated event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "NotAuthenticated");
}

TEST(event, AlreadyAuthenticated_event_to_json) {
  event::AlreadyAuthenticated event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "AlreadyAuthenticated");
}

TEST(event, UpdatesReceived_event_to_json) {
  data::Package p1;
  p1.name = "packagename1";
  p1.version = "v2.0";
  data::UpdateRequest ur1, ur2;
  ur1.requestId = "id1";
  ur1.status = data::Pending;
  ur1.packageId = p1;
  ur1.installPos = 3;
  ur1.createdAt = "today";

  std::vector<data::UpdateRequest> update_requests;
  update_requests.push_back(ur1);

  event::UpdatesReceived event(update_requests);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "UpdatesReceived");
  EXPECT_EQ(json["fields"][0][0]["requestId"].asString(), "id1");
  EXPECT_EQ(json["fields"][0][0]["status"].asString(), "Pending");
  EXPECT_EQ(json["fields"][0][0]["installPos"].asUInt(), 3u);
  EXPECT_EQ(json["fields"][0][0]["createdAt"].asString(), "today");
}

TEST(event, UpdatesReceived_parse) {
  std::string message(
      "[ "
      "{\n"
      "   \"createdAt\" : \"2017-01-05T08:43:40.685Z\",\n"
      "		\"installPos\" : 0,\n"
      "		\"packageId\" : \n"
      "		{\n"
      "			\"name\" : \"treehub-ota-raspberrypi3\",\n"
      "			\"version\" : "
      "\"15f13e2582f29d2218f88adada52ff043d6e9596b7cea288321ed91e275a1768\"\n"
      "		},\n"
      "		\"requestId\" : \"06d64e46-cb25-4d76-b62e-4341b6944d07\",\n"
      "		\"status\" : \"Pending\",\n"
      "		\"updatedAt\" : \"2017-01-05T08:43:40.685Z\"\n"
      "	}\n"
      "]");

  Json::Reader reader;
  Json::Value json;
  reader.parse(message, json);

  data::UpdateRequest request(data::UpdateRequest::fromJson(json[0]));
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

TEST(event, NoUpdateRequests_event_to_json) {
  event::NoUpdateRequests event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "NoUpdateRequests");
}

TEST(event, FoundInstalledPackages_event_to_json) {
  data::Package p1, p2;
  p1.name = "packagename1";
  p1.version = "v2.0";
  p2.name = "packagename2";
  p2.version = "v3.0";

  std::vector<data::Package> packages;
  packages.push_back(p1);
  packages.push_back(p2);

  event::FoundInstalledPackages event(packages);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "FoundInstalledPackages");
  EXPECT_EQ(json["fields"][0][0]["name"].asString(), "packagename1");
  EXPECT_EQ(json["fields"][0][0]["version"].asString(), "v2.0");
  EXPECT_EQ(json["fields"][0][1]["name"].asString(), "packagename2");
  EXPECT_EQ(json["fields"][0][1]["version"].asString(), "v3.0");
}

TEST(event, FoundSystemInfo_event_to_json) {
  event::FoundSystemInfo event("testinfo");

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "FoundSystemInfo");
  EXPECT_EQ(json["fields"][0].asString(), "testinfo");
}

TEST(event, DownloadingUpdate_event_to_json) {
  event::DownloadingUpdate event("testid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "DownloadingUpdate");
  EXPECT_EQ(json["fields"][0].asString(), "testid");
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

TEST(event, DownloadFailed_event_to_json) {
  event::DownloadFailed event("requestid", "description");

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "DownloadFailed");
  EXPECT_EQ(json["fields"][0].asString(), "requestid");
  EXPECT_EQ(json["fields"][1].asString(), "description");
}

TEST(event, InstallingUpdate_event_to_json) {
  event::InstallingUpdate event("requestid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "InstallingUpdate");
  EXPECT_EQ(json["fields"][0].asString(), "requestid");
}

TEST(event, InstallComplete_event_to_json) {
  data::OperationResult operation_result;
  operation_result.id = "testid23";
  operation_result.result_code = data::NOT_FOUND;
  operation_result.result_text = "text";

  event::InstallComplete event(operation_result);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "InstallComplete");
  EXPECT_EQ(json["fields"][0]["id"].asString(), "testid23");
  EXPECT_EQ(json["fields"][0]["result_code"].asUInt(), data::NOT_FOUND);
  EXPECT_EQ(json["fields"][0]["result_text"].asString(), "text");
}

TEST(event, InstallFailed_event_to_json) {
  data::OperationResult operation_result;
  operation_result.id = "testid23";
  operation_result.result_code = data::NOT_FOUND;
  operation_result.result_text = "text";

  event::InstallFailed event(operation_result);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "InstallFailed");
  EXPECT_EQ(json["fields"][0]["id"].asString(), "testid23");
  EXPECT_EQ(json["fields"][0]["result_code"].asUInt(), data::NOT_FOUND);
  EXPECT_EQ(json["fields"][0]["result_text"].asString(), "text");
}

TEST(event, UpdateReportSent_event_to_json) {
  event::UpdateReportSent event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "UpdateReportSent");
}

TEST(event, InstalledPackagesSent_event_to_json) {
  event::InstalledPackagesSent event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "InstalledPackagesSent");
}

TEST(event, InstalledSoftwareSent_event_to_json) {
  event::InstalledSoftwareSent event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "InstalledSoftwareSent");
}

TEST(event, SystemInfoSent_event_to_json) {
  event::SystemInfoSent event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "SystemInfoSent");
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
#ifdef BUILD_OSTREE

TEST(event, UptaneTargetsUpdated_event_to_json) {
  OstreePackage package("test1", "test2", "test3", "test4");
  std::vector<OstreePackage> packages;
  packages.push_back(package);

  event::UptaneTargetsUpdated event(packages);
  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  EXPECT_EQ(json["variant"].asString(), "UptaneTargetsUpdated");
}
#endif

TEST(event, Error_event_from_json) {
  std::string json = "{\"fields\":[\"Error123\"],\"variant\":\"Error\"}";
  event::Error event = event::Error::fromJson(json);
  EXPECT_EQ(event.variant, "Error");
  EXPECT_EQ(event.message, "Error123");
}

TEST(event, UpdatesReceived_event_from_json) {
  std::string json =
      "{\"fields\":[[{\"createdAt\":\"today\",\"installPos\":3,"
      "\"packageId\":{"
      "\"name\":\"packagename1\",\"version\":\"v2.0\"},"
      "\"requestId\":\"id1\","
      "\"status\":\"Pending\"}]],\"variant\":\"UpdatesReceived\"}";
  event::UpdatesReceived event = event::UpdatesReceived::fromJson(json);

  EXPECT_EQ(event.variant, "UpdatesReceived");
  EXPECT_EQ(event.update_requests[0].requestId, "id1");
  EXPECT_EQ(event.update_requests[0].createdAt, "today");
  EXPECT_EQ(event.update_requests[0].installPos, 3u);
  EXPECT_EQ(event.update_requests[0].status, data::Pending);
  EXPECT_EQ(event.update_requests[0].packageId.name, "packagename1");
  EXPECT_EQ(event.update_requests[0].packageId.version, "v2.0");
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

TEST(event, FoundInstalledPackages_event_from_json) {
  std::string json =
      "{\"fields\":[[{\"name\":\"packagename1\",\"version\":\"v2.0\"},{"
      "\"name\":\"packagename2\",\"version\":\"v3.0\"}]],\"variant\":"
      "\"FoundInstalledPackages\"}";
  event::FoundInstalledPackages event = event::FoundInstalledPackages::fromJson(json);

  EXPECT_EQ(event.variant, "FoundInstalledPackages");
  EXPECT_EQ(event.packages[0].name, "packagename1");
  EXPECT_EQ(event.packages[0].version, "v2.0");
  EXPECT_EQ(event.packages[1].name, "packagename2");
  EXPECT_EQ(event.packages[1].version, "v3.0");
}

TEST(event, FoundSystemInfo_event_from_json) {
  std::string json = "{\"fields\":[\"info\"],\"variant\":\"FoundSystemInfo\"}";
  event::FoundSystemInfo event = event::FoundSystemInfo::fromJson(json);

  EXPECT_EQ(event.variant, "FoundSystemInfo");
  EXPECT_EQ(event.info, "info");
}

TEST(event, DownloadingUpdate_event_from_json) {
  std::string json = "{\"fields\":[\"testid\"],\"variant\":\"DownloadingUpdate\"}";
  event::DownloadingUpdate event = event::DownloadingUpdate::fromJson(json);

  EXPECT_EQ(event.variant, "DownloadingUpdate");
  EXPECT_EQ(event.update_request_id, "testid");
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

TEST(event, DownloadFailed_event_from_json) {
  std::string json =
      "{\"fields\":[\"requestid\",\"description\"],\"variant\":"
      "\"DownloadFailed\"}";
  event::DownloadFailed event = event::DownloadFailed::fromJson(json);

  EXPECT_EQ(event.variant, "DownloadFailed");
  EXPECT_EQ(event.update_request_id, "requestid");
  EXPECT_EQ(event.message, "description");
}

TEST(event, InstallingUpdate_event_from_json) {
  std::string json = "{\"fields\":[\"requestid\"],\"variant\":\"InstallingUpdate\"}";
  event::InstallingUpdate event = event::InstallingUpdate::fromJson(json);

  EXPECT_EQ(event.variant, "InstallingUpdate");
  EXPECT_EQ(event.update_request_id, "requestid");
}

TEST(event, InstallComplete_event_from_json) {
  std::string json =
      "{\"fields\":[{\"id\":\"testid23\",\"result_"
      "code\":16,\"result_text\":\"text\"}],"
      "\"variant\":\"InstallComplete\"}";
  event::InstallComplete event = event::InstallComplete::fromJson(json);

  EXPECT_EQ(event.variant, "InstallComplete");
  EXPECT_EQ(event.install_result.id, "testid23");
  EXPECT_EQ(event.install_result.result_code, 16);
  EXPECT_EQ(event.install_result.result_text, "text");
}

TEST(event, InstallFailed_event_from_json) {
  std::string json =
      "{\"fields\":[{\"id\":\"testid23\",\"result_"
      "code\":16,\"result_text\":\"text\"}],"
      "\"variant\":\"InstallFailed\"}";
  event::InstallFailed event = event::InstallFailed::fromJson(json);

  EXPECT_EQ(event.variant, "InstallFailed");
  EXPECT_EQ(event.install_result.id, "testid23");
  EXPECT_EQ(event.install_result.result_code, 16);
  EXPECT_EQ(event.install_result.result_text, "text");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
