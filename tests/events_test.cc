#define BOOST_TEST_MODULE test_events

#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test.hpp>
#include <string>

#include "events.h"

BOOST_AUTO_TEST_CASE(Error_event_to_json) {
  std::string error = "Error123";
  event::Error event(error);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "Error");
  BOOST_CHECK_EQUAL(json["fields"][0].asString(), error);
}

BOOST_AUTO_TEST_CASE(Authenticated_event_to_json) {
  event::Authenticated event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "Authenticated");
}

BOOST_AUTO_TEST_CASE(NotAuthenticated_event_to_json) {
  event::NotAuthenticated event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "NotAuthenticated");
}

BOOST_AUTO_TEST_CASE(AlreadyAuthenticated_event_to_json) {
  event::AlreadyAuthenticated event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "AlreadyAuthenticated");
}

BOOST_AUTO_TEST_CASE(UpdatesReceived_event_to_json) {
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

  BOOST_CHECK_EQUAL(json["variant"].asString(), "UpdatesReceived");
  BOOST_CHECK_EQUAL(json["fields"][0][0]["requestId"].asString(), "id1");
  BOOST_CHECK_EQUAL(json["fields"][0][0]["status"].asUInt(), data::Pending);
  BOOST_CHECK_EQUAL(json["fields"][0][0]["installPos"].asUInt(), 3);
  BOOST_CHECK_EQUAL(json["fields"][0][0]["createdAt"].asString(), "today");
}

BOOST_AUTO_TEST_CASE(UpdateAvailable_event_to_json) {
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

  BOOST_CHECK_EQUAL(json["variant"].asString(), "UpdateAvailable");
  BOOST_CHECK_EQUAL(json["fields"][0]["update_id"].asString(), "id4");
  BOOST_CHECK_EQUAL(json["fields"][0]["signature"].asString(), "sign");
  BOOST_CHECK_EQUAL(json["fields"][0]["description"].asBool(), true);
}

BOOST_AUTO_TEST_CASE(NoUpdateRequests_event_to_json) {
  event::NoUpdateRequests event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "NoUpdateRequests");
}

BOOST_AUTO_TEST_CASE(FoundInstalledPackages_event_to_json) {
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

  BOOST_CHECK_EQUAL(json["variant"].asString(), "FoundInstalledPackages");
  BOOST_CHECK_EQUAL(json["fields"][0][0]["name"].asString(), "packagename1");
  BOOST_CHECK_EQUAL(json["fields"][0][0]["version"].asString(), "v2.0");
  BOOST_CHECK_EQUAL(json["fields"][0][1]["name"].asString(), "packagename2");
  BOOST_CHECK_EQUAL(json["fields"][0][1]["version"].asString(), "v3.0");
}

BOOST_AUTO_TEST_CASE(FoundSystemInfo_event_to_json) {
  event::FoundSystemInfo event("testinfo");

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "FoundSystemInfo");
  BOOST_CHECK_EQUAL(json["fields"][0].asString(), "testinfo");
}

BOOST_AUTO_TEST_CASE(DownloadingUpdate_event_to_json) {
  event::DownloadingUpdate event("testid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "DownloadingUpdate");
  BOOST_CHECK_EQUAL(json["fields"][0].asString(), "testid");
}

BOOST_AUTO_TEST_CASE(DownloadComplete_event_to_json) {
  data::DownloadComplete dc;
  dc.update_id = "updateid123";
  dc.update_image = "some text";
  dc.signature = "sign";

  event::DownloadComplete event(dc);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "DownloadComplete");
  BOOST_CHECK_EQUAL(json["fields"][0]["update_id"].asString(), "updateid123");
  BOOST_CHECK_EQUAL(json["fields"][0]["update_image"].asString(), "some text");
  BOOST_CHECK_EQUAL(json["fields"][0]["signature"].asString(), "sign");
}

BOOST_AUTO_TEST_CASE(DownloadFailed_event_to_json) {
  event::DownloadFailed event("requestid", "description");

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "DownloadFailed");
  BOOST_CHECK_EQUAL(json["fields"][0].asString(), "requestid");
  BOOST_CHECK_EQUAL(json["fields"][1].asString(), "description");
}

BOOST_AUTO_TEST_CASE(InstallingUpdate_event_to_json) {
  event::InstallingUpdate event("requestid");

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "InstallingUpdate");
  BOOST_CHECK_EQUAL(json["fields"][0].asString(), "requestid");
}

BOOST_AUTO_TEST_CASE(InstallComplete_event_to_json) {
  data::OperationResult operation_result;
  operation_result.id = "testid23";
  operation_result.result_code = data::NOT_FOUND;
  operation_result.result_text = "text";
  std::vector<data::OperationResult> operation_results;
  operation_results.push_back(operation_result);
  data::UpdateReport ur;
  ur.update_id = "request_id";
  ur.operation_results = operation_results;

  event::InstallComplete event(ur);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "InstallComplete");
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

BOOST_AUTO_TEST_CASE(InstallFailed_event_to_json) {
  data::OperationResult operation_result;
  operation_result.id = "testid23";
  operation_result.result_code = data::NOT_FOUND;
  operation_result.result_text = "text";
  std::vector<data::OperationResult> operation_results;
  operation_results.push_back(operation_result);
  data::UpdateReport ur;
  ur.update_id = "request_id";
  ur.operation_results = operation_results;

  event::InstallFailed event(ur);

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "InstallFailed");
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

BOOST_AUTO_TEST_CASE(UpdateReportSent_event_to_json) {
  event::UpdateReportSent event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "UpdateReportSent");
}

BOOST_AUTO_TEST_CASE(InstalledPackagesSent_event_to_json) {
  event::InstalledPackagesSent event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "InstalledPackagesSent");
}

BOOST_AUTO_TEST_CASE(InstalledSoftwareSent_event_to_json) {
  event::InstalledSoftwareSent event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "InstalledSoftwareSent");
}

BOOST_AUTO_TEST_CASE(SystemInfoSent_event_to_json) {
  event::SystemInfoSent event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "SystemInfoSent");
}

BOOST_AUTO_TEST_CASE(InstalledSoftwareNeeded_event_to_json) {
  event::InstalledSoftwareNeeded event;

  Json::Reader reader;
  Json::Value json;
  reader.parse(event.toJson(), json);

  BOOST_CHECK_EQUAL(json["variant"].asString(), "InstalledSoftwareNeeded");
}

BOOST_AUTO_TEST_CASE(Error_event_from_json) {
  std::string json = "{\"fields\":[\"Error123\"],\"variant\":\"Error\"}";
  event::Error event = event::Error::fromJson(json);
  BOOST_CHECK_EQUAL(event.variant, "Error");
  BOOST_CHECK_EQUAL(event.message, "Error123");
}

BOOST_AUTO_TEST_CASE(UpdatesReceived_event_from_json) {
  std::string json =
      "{\"fields\":[[{\"createdAt\":\"today\",\"installPos\":3,\"packageId\":{"
      "\"name\":\"packagename1\",\"version\":\"v2.0\"},\"requestId\":\"id1\","
      "\"status\":0}]],\"variant\":\"UpdatesReceived\"}";
  event::UpdatesReceived event = event::UpdatesReceived::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "UpdatesReceived");
  BOOST_CHECK_EQUAL(event.update_requests[0].requestId, "id1");
  BOOST_CHECK_EQUAL(event.update_requests[0].createdAt, "today");
  BOOST_CHECK_EQUAL(event.update_requests[0].installPos, 3);
  BOOST_CHECK_EQUAL(event.update_requests[0].status, 0);
  BOOST_CHECK_EQUAL(event.update_requests[0].packageId.name, "packagename1");
  BOOST_CHECK_EQUAL(event.update_requests[0].packageId.version, "v2.0");
}

BOOST_AUTO_TEST_CASE(UpdateAvailable_event_from_json) {
  std::string json =
      "{\"fields\":[{\"description\":\"this is "
      "description\",\"request_confirmation\":true,\"signature\":\"sign\","
      "\"size\":5000,\"update_id\":\"id4\"}],\"variant\":\"UpdateAvailable\"}";
  event::UpdateAvailable event = event::UpdateAvailable::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "UpdateAvailable");
  BOOST_CHECK_EQUAL(event.update_vailable.description, "this is description");
  BOOST_CHECK_EQUAL(event.update_vailable.request_confirmation, true);
  BOOST_CHECK_EQUAL(event.update_vailable.signature, "sign");
  BOOST_CHECK_EQUAL(event.update_vailable.size, 5000);
  BOOST_CHECK_EQUAL(event.update_vailable.update_id, "id4");
}

BOOST_AUTO_TEST_CASE(FoundInstalledPackages_event_from_json) {
  std::string json =
      "{\"fields\":[[{\"name\":\"packagename1\",\"version\":\"v2.0\"},{"
      "\"name\":\"packagename2\",\"version\":\"v3.0\"}]],\"variant\":"
      "\"FoundInstalledPackages\"}";
  event::FoundInstalledPackages event =
      event::FoundInstalledPackages::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "FoundInstalledPackages");
  BOOST_CHECK_EQUAL(event.packages[0].name, "packagename1");
  BOOST_CHECK_EQUAL(event.packages[0].version, "v2.0");
  BOOST_CHECK_EQUAL(event.packages[1].name, "packagename2");
  BOOST_CHECK_EQUAL(event.packages[1].version, "v3.0");
}

BOOST_AUTO_TEST_CASE(FoundSystemInfo_event_from_json) {
  std::string json = "{\"fields\":[\"info\"],\"variant\":\"FoundSystemInfo\"}";
  event::FoundSystemInfo event = event::FoundSystemInfo::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "FoundSystemInfo");
  BOOST_CHECK_EQUAL(event.info, "info");
}

BOOST_AUTO_TEST_CASE(DownloadingUpdate_event_from_json) {
  std::string json =
      "{\"fields\":[\"testid\"],\"variant\":\"DownloadingUpdate\"}";
  event::DownloadingUpdate event = event::DownloadingUpdate::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "DownloadingUpdate");
  BOOST_CHECK_EQUAL(event.update_request_id, "testid");
}

BOOST_AUTO_TEST_CASE(DownloadComplete_event_from_json) {
  std::string json =
      "{\"fields\":[{\"signature\":\"sign\",\"update_id\":\"updateid123\","
      "\"update_image\":\"some text\"}],\"variant\":\"DownloadComplete\"}";
  event::DownloadComplete event = event::DownloadComplete::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "DownloadComplete");
  BOOST_CHECK_EQUAL(event.download_complete.signature, "sign");
  BOOST_CHECK_EQUAL(event.download_complete.update_id, "updateid123");
  BOOST_CHECK_EQUAL(event.download_complete.update_image, "some text");
}

BOOST_AUTO_TEST_CASE(DownloadFailed_event_from_json) {
  std::string json =
      "{\"fields\":[\"requestid\",\"description\"],\"variant\":"
      "\"DownloadFailed\"}";
  event::DownloadFailed event = event::DownloadFailed::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "DownloadFailed");
  BOOST_CHECK_EQUAL(event.update_request_id, "requestid");
  BOOST_CHECK_EQUAL(event.message, "description");
}

BOOST_AUTO_TEST_CASE(InstallingUpdate_event_from_json) {
  std::string json =
      "{\"fields\":[\"requestid\"],\"variant\":\"InstallingUpdate\"}";
  event::InstallingUpdate event = event::InstallingUpdate::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "InstallingUpdate");
  BOOST_CHECK_EQUAL(event.update_request_id, "requestid");
}

BOOST_AUTO_TEST_CASE(InstallComplete_event_from_json) {
  std::string json =
      "{\"fields\":[{\"operation_results\":[{\"id\":\"testid23\",\"result_"
      "code\":16,\"result_text\":\"text\"}],\"update_id\":\"request_id\"}],"
      "\"variant\":\"InstallComplete\"}";
  event::InstallComplete event = event::InstallComplete::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "InstallComplete");
  BOOST_CHECK_EQUAL(event.update_report.operation_results[0].id, "testid23");
  BOOST_CHECK_EQUAL(event.update_report.operation_results[0].result_code, 16);
  BOOST_CHECK_EQUAL(event.update_report.operation_results[0].result_text,
                    "text");
  BOOST_CHECK_EQUAL(event.update_report.update_id, "request_id");
}

BOOST_AUTO_TEST_CASE(InstallFailed_event_from_json) {
  std::string json =
      "{\"fields\":[{\"operation_results\":[{\"id\":\"testid23\",\"result_"
      "code\":16,\"result_text\":\"text\"}],\"update_id\":\"request_id\"}],"
      "\"variant\":\"InstallFailed\"}";
  event::InstallFailed event = event::InstallFailed::fromJson(json);

  BOOST_CHECK_EQUAL(event.variant, "InstallFailed");
  BOOST_CHECK_EQUAL(event.update_report.operation_results[0].id, "testid23");
  BOOST_CHECK_EQUAL(event.update_report.operation_results[0].result_code, 16);
  BOOST_CHECK_EQUAL(event.update_report.operation_results[0].result_text,
                    "text");
  BOOST_CHECK_EQUAL(event.update_report.update_id, "request_id");
}
