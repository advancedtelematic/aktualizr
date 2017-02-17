#include "sotahttpclient.h"

#include <json/json.h>
#include <boost/make_shared.hpp>
#include "time.h"

#include "logger.h"

SotaHttpClient::SotaHttpClient(const Config &config_in, event::Channel *events_channel_in,
                               command::Channel *commands_channel_in)
    : config(config_in), events_channel(events_channel_in), commands_channel(commands_channel_in) {
  http = new HttpClient();
  core_url = config.core.server + "/api/v1";
  processing = false;
  retries = SotaHttpClient::MAX_RETRIES;
  boost::thread(boost::bind(&SotaHttpClient::run, this));
}

SotaHttpClient::~SotaHttpClient() { delete http; }

SotaHttpClient::SotaHttpClient(const Config &config_in, HttpClient *http_in, event::Channel *events_channel_in,
                               command::Channel *commands_channel_in)
    : config(config_in), http(http_in), events_channel(events_channel_in), commands_channel(commands_channel_in) {
  core_url = config.core.server + "/api/v1";
}

bool SotaHttpClient::authenticate() { return http->authenticate(config.auth); }

std::vector<data::UpdateRequest> SotaHttpClient::getAvailableUpdates() {
  processing = true;
  std::string url = core_url + "/mydevice/" + config.device.uuid + "/updates";
  std::vector<data::UpdateRequest> update_requests;

  Json::Value json;
  try {
    json = http->get(url);
  } catch (std::runtime_error e) {
    retry();
    return update_requests;
  }
  unsigned int updates = json.size();
  for (unsigned int i = 0; i < updates; ++i) {
    update_requests.push_back(data::UpdateRequest::fromJson(json[i]));
  }
  if (!updates) {
    processing = false;
  }
  return update_requests;
}

void SotaHttpClient::startDownload(const data::UpdateRequestId &update_request_id) {
  std::string url = core_url + "/mydevice/" + config.device.uuid + "/updates/" + update_request_id + "/download";
  std::string filename = config.device.packages_dir + update_request_id;

  if (!http->download(url, filename)) {
    retry();
    return;
  }

  data::DownloadComplete download_complete;
  download_complete.update_id = update_request_id;
  download_complete.signature = "";
  download_complete.update_image = filename;
  *events_channel << boost::make_shared<event::DownloadComplete>(download_complete);
}

void SotaHttpClient::sendUpdateReport(data::UpdateReport update_report) {
  std::string url = core_url + "/mydevice/" + config.device.uuid;
  url += "/updates/" + update_report.update_id;
  try {
    http->post(url, update_report.toJson()["operation_results"]);
  } catch (std::runtime_error e) {
    retry();
    return;
  }
  processing = true;
}

void SotaHttpClient::retry() {
  was_error = true;
  processing = false;
  retries--;
  if (!retries) {
    retries = SotaHttpClient::MAX_RETRIES;
    was_error = false;
  }
}

void SotaHttpClient::run() {
  while (true) {
    if (processing) {
      sleep(1);
    } else {
      if (was_error) {
        sleep(2);
      } else {
        sleep((unsigned int)config.core.polling_sec);
      }
      if (!http->isAuthenticated()) {
        data::ClientCredentials cred;
        cred.client_id = config.auth.client_id;
        cred.client_secret = config.auth.client_secret;
        *commands_channel << boost::make_shared<command::Authenticate>(cred);
      }
      *commands_channel << boost::make_shared<command::GetUpdateRequests>();
    }
  }
}
