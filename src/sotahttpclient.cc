#include "sotahttpclient.h"

#include <json/json.h>
#include "time.h"

#include "logger.h"

SotaHttpClient::SotaHttpClient(const Config &config_in) : config(config_in) {
  http = new HttpClient();
  http->authenticate(config.auth);
  core_url = config.core.server + "/api/v1";
}

SotaHttpClient::~SotaHttpClient() {
  delete http;
}

SotaHttpClient::SotaHttpClient(const Config &config_in, HttpClient *http_in)
    : config(config_in), http(http_in) {
  http->authenticate(config.auth);
  core_url = config.core.server + "/api/v1";
}

std::vector<data::UpdateRequest> SotaHttpClient::getAvailableUpdates() {
  std::string url = core_url + "/mydevice/" + config.device.uuid + "/updates";
  std::vector<data::UpdateRequest> update_requests;

  Json::Value json = http->get(url);
  for (unsigned int i = 0; i < json.size(); ++i) {
    update_requests.push_back(data::UpdateRequest::fromJson(json[i]));
  }
  return update_requests;
}

Json::Value SotaHttpClient::downloadUpdate(
    const data::UpdateRequestId &update_request_id) {
  std::string url = core_url + "/mydevice/" + config.device.uuid + "/updates/" +
                    update_request_id + "/download";
  std::string filename = config.device.packages_dir + update_request_id;
  http->download(url, filename);
  Json::Value status;
  status["updateId"] = update_request_id;
  status["resultCode"] = 0;
  status["resultText"] = "Downloaded";

  time_t _tm = time(NULL);
  tm *curtime = localtime(&_tm);
  status["receivedAt"] = asctime(curtime);
  return status;
}

Json::Value SotaHttpClient::reportUpdateResult(
    data::UpdateReport &update_report) {
  std::string url = core_url + "/mydevice/" + config.device.uuid;
  url += "/updates/" + update_report.update_id;
  return http->post(url, update_report.toJson()["operation_results"]);
}
