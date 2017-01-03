#include "sotahttpclient.h"

#include <json/json.h>
#include "time.h"

#include "logger.h"

SotaHttpClient::SotaHttpClient(const Config &config_in) : config(config_in) {
  http.authenticate(config.auth);
  core_url = config.core.server + "/api/v1";
}

std::vector<data::UpdateRequest> SotaHttpClient::getAvailableUpdates() {
  std::string url =
      core_url + "/device_updates/" + config.device.uuid + "/queued";
  std::vector<data::UpdateRequest> update_requests;

  Json::Value json = http.get(url);
  for (int i = 0; json.size(); ++i) {
    update_requests.push_back(data::UpdateRequest::fromJson(json[i]));
  }
  return update_requests;
}

Json::Value SotaHttpClient::downloadUpdate(
    const data::UpdateRequest &update_request) {
  std::string url = core_url + "/device_updates/" + config.device.uuid + "/" +
                    update_request.requestId + "/download";
  std::string filename =
      config.device.packages_dir + update_request.packageId.name;
  http.download(url, filename);
  Json::Value status;
  status["updateId"] = update_request.requestId;
  status["resultCode"] = 0;
  status["resultText"] = "Downloaded";

  time_t _tm = time(NULL);
  tm *curtime = localtime(&_tm);
  status["receivedAt"] = asctime(curtime);
  return status;
}

Json::Value SotaHttpClient::reportUpdateResult(
    const Json::Value &download_status) {
  std::string url = core_url + "/device_updates/" + config.device.uuid;
  std::stringstream ss;
  ss << "[" << download_status << "]";
  return http.post(url, ss.str());
}
