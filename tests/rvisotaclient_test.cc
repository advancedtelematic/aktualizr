#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config.h"
#include "json/json.h"
#include "sotarviclient.h"
#include "types.h"

void callbackWrapper(int fd, void *service_data, const char *parameters);

TRviHandle rviJsonInit(char *configFilename) {
  (void)configFilename;
  TRviHandle handle = (void *)1;
  return handle;
}

void rviSetVerboseLogs(bool verboseEnable) { (void)verboseEnable; }

int rviConnect(TRviHandle handle, const char *addr, const char *port) {
  (void)handle;
  (void)addr;
  (void)port;
  return 2;
}

int rviRegisterService(TRviHandle handle, const char *serviceName, TRviCallback callback, void *serviceData) {
  (void)handle;
  (void)serviceName;
  (void)callback;
  (void)serviceData;
  return 0;
}

int rviProcessInput(TRviHandle handle, int *fdArr, int fdLen) {
  (void)handle;
  (void)fdArr;
  (void)fdLen;
  sleep(1);
  return 0;
}

int rviInvokeService(TRviHandle handle, const char *serviceName, const char *parameters) {
  (void)handle;
  Json::Reader reader;
  Json::Value json;
  reader.parse(parameters, json);

  std::string service(serviceName);
  if (service == "genivi.org/backend/sota/start") {
    throw std::string("start called");
  } else if (service == "genivi.org/backend/sota/ack") {
    throw std::string("ack called");
  } else if (service == "genivi.org/backend/sota/report") {
    throw std::string("report called");
  }
  throw std::string("Error");
  return 0;
}

int rviCleanup(TRviHandle handle) {
  (void)handle;
  return 0;
}

TEST(NotifyTest, UpdateAvailable_send) {
  Config conf;
  event::Channel chann;
  SotaRVIClient client(conf, &chann);
  void *pointers[2];
  pointers[0] = (void *)(&client);
  char event[] = "notify";
  pointers[1] = event;

  Json::Value value;
  value["update_available"]["update_id"] = "test_update_id";
  value["update_available"]["signature"] = "123";
  value["update_available"]["description"] = "description";
  value["update_available"]["request_confirmation"] = false;
  value["update_available"]["size"] = 100;

  Json::Value params(Json::arrayValue);
  params.append(value);

  callbackWrapper(1, pointers, Json::FastWriter().write(params).c_str());
  boost::shared_ptr<event::BaseEvent> ev;
  chann >> ev;
  EXPECT_EQ(ev->variant, "UpdateAvailable");
  EXPECT_EQ(static_cast<event::UpdateAvailable *>(ev.get())->update_vailable.update_id, "test_update_id");
}

TEST(DownloadTest, download_started) {
  Config conf;
  event::Channel chann;
  SotaRVIClient client(conf, &chann);
  try {
    client.startDownload("test_update_id");
  } catch (std::string result) {
    EXPECT_EQ(result, "start called");
  }
}

TEST(AckTest, ack_called) {
  Config conf;
  event::Channel chann;
  SotaRVIClient client(conf, &chann);
  void *pointers[2];
  pointers[0] = (void *)(&client);
  char event[] = "start";
  pointers[1] = event;

  Json::Value value;
  value["update_id"] = "test_update_id";

  Json::Value params(Json::arrayValue);
  params.append(value);

  try {
    callbackWrapper(1, pointers, Json::FastWriter().write(params).c_str());
  } catch (std::string result) {
    EXPECT_EQ(result, "ack called");
  }
}

TEST(SaveTest, file_saved) {
  Config conf;
  event::Channel chann;
  SotaRVIClient client(conf, &chann);
  void *pointers[2];
  pointers[0] = (void *)(&client);
  char event[] = "chunk";
  pointers[1] = event;

  Json::Value value;
  value["update_id"] = "test_update_id";
  value["bytes"] = "dGVzdG1lc3NhZ2U=";
  value["index"] = 0;

  Json::Value params(Json::arrayValue);
  params.append(value);

  try {
    callbackWrapper(1, pointers, Json::FastWriter().write(params).c_str());
  } catch (std::string result) {
    EXPECT_EQ(result, "ack called");
  }
  std::ifstream saved_file((conf.device.packages_dir + "test_update_id").c_str());
  std::string content;
  saved_file >> content;
  std::cout << content;
  saved_file.close();
  remove((conf.device.packages_dir + "test_update_id").c_str());
  EXPECT_EQ(content, "testmessage");
}

TEST(FinisTest, DownloadComplete_send) {
  Config conf;
  event::Channel chann;
  SotaRVIClient client(conf, &chann);
  void *pointers[2];
  pointers[0] = (void *)(&client);
  char event[] = "finish";
  pointers[1] = event;

  Json::Value value;
  value["update_id"] = "test_update_id";
  value["signature"] = "";
  value["image"] = "filename";

  Json::Value params(Json::arrayValue);
  params.append(value);

  callbackWrapper(1, pointers, Json::FastWriter().write(params).c_str());
  boost::shared_ptr<event::BaseEvent> ev;
  chann >> ev;
  EXPECT_EQ(ev->variant, "DownloadComplete");
  EXPECT_EQ(static_cast<event::DownloadComplete *>(ev.get())->download_complete.update_id, "test_update_id");
}

TEST(ReportTest, report_called) {
  Config conf;
  event::Channel chann;
  SotaRVIClient client(conf, &chann);

  data::OperationResult op_res;
  op_res.id = "123";
  op_res.result_code = data::OK;
  op_res.result_text = "good";
  data::UpdateReport ur;
  ur.operation_results.push_back(op_res);
  ur.update_id = "update_id";

  try {
    client.sendUpdateReport(ur);
  } catch (std::string message) {
    EXPECT_EQ(message, "report called");
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
