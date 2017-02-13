#include "sotahttpclient.h"

#include <json/json.h>
#include <boost/make_shared.hpp>
#include "time.h"

#include "logger.h"

SotaHttpClient::SotaHttpClient(const Config &config_in,
                               event::Channel *events_channel_in,
                               command::Channel *commands_channel_in)
    : config(config_in),
      events_channel(events_channel_in),
      commands_channel(commands_channel_in) {
  http = new HttpClient();
  http->authenticate(config.auth);
  core_url = config.core.server + "/api/v1";
  boost::thread(boost::bind(&SotaHttpClient::run, this));
}

SotaHttpClient::~SotaHttpClient() { delete http; }

SotaHttpClient::SotaHttpClient(const Config &config_in, HttpClient *http_in,
                               event::Channel *events_channel_in,
                               command::Channel *commands_channel_in)
    : config(config_in),
      http(http_in),
      events_channel(events_channel_in),
      commands_channel(commands_channel_in) {
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

void SotaHttpClient::startDownload(
    const data::UpdateRequestId &update_request_id) {
  std::string url = core_url + "/mydevice/" + config.device.uuid + "/updates/" +
                    update_request_id + "/download";
  std::string filename = config.device.packages_dir + update_request_id;

  // TODO handle errors here
  http->download(url, filename);

  data::DownloadComplete download_complete;
  download_complete.update_id = update_request_id;
  download_complete.signature = "";
  download_complete.update_image = filename;
  *events_channel << boost::make_shared<event::DownloadComplete>(
      download_complete);
}

void SotaHttpClient::sendUpdateReport(data::UpdateReport update_report) {
  std::string url = core_url + "/mydevice/" + config.device.uuid;
  url += "/updates/" + update_report.update_id;
  http->post(url, update_report.toJson()["operation_results"]);
}

void SotaHttpClient::run() {
  while (true) {
    *commands_channel << boost::make_shared<command::GetUpdateRequests>();
    sleep(static_cast<unsigned int>(config.core.polling_sec));
  }
}
