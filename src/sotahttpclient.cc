#include "sotahttpclient.h"

#include <json/json.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/make_shared.hpp>
#include "time.h"

#include "crypto.h"
#include "logger.h"

SotaHttpClient::SotaHttpClient(const Config &config_in, event::Channel *events_channel_in)
    : config(config_in), events_channel(events_channel_in) {
  http = new HttpClient();

  core_url = config.core.server + "/api/v1";

  processing = false;
  retries = SotaHttpClient::MAX_RETRIES;
}

SotaHttpClient::~SotaHttpClient() { delete http; }

SotaHttpClient::SotaHttpClient(const Config &config_in, HttpClient *http_in, event::Channel *events_channel_in)
    : config(config_in), http(http_in), events_channel(events_channel_in) {
  core_url = config.core.server + "/api/v1";
}

bool SotaHttpClient::authenticate() { return http->authenticate(config.auth); }

std::vector<data::UpdateRequest> SotaHttpClient::getAvailableUpdates() {
  processing = true;
  std::string url = core_url + "/mydevice/" + config.device.uuid + "/updates";
  std::vector<data::UpdateRequest> update_requests;

  Json::Value json;
  try {
    json = http->getJson(url);
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

void SotaHttpClient::run(command::Channel *commands_channel) {
  while (true) {
    if (processing) {
      sleep(1);
    } else {
      if (was_error) {
        sleep(2);
      } else {
        sleep(static_cast<unsigned int>(config.core.polling_sec));
      }
      if (!http->isAuthenticated()) {
        *commands_channel << boost::make_shared<command::Authenticate>();
      }
      *commands_channel << boost::make_shared<command::GetUpdateRequests>();
    }
  }
}

void SotaHttpClient::runForever(command::Channel *commands_channel) {
  boost::thread(boost::bind(&SotaHttpClient::run, this, commands_channel));
  boost::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    LOGGER_LOG(LVL_info, "got " + command->variant + " command");
    if (command->variant == "Authenticate") {
      if (authenticate()) {
        *events_channel << boost::make_shared<event::Authenticated>();
      } else {
        *events_channel << boost::make_shared<event::NotAuthenticated>();
      }
    } else if (command->variant == "GetUpdateRequests") {
      std::vector<data::UpdateRequest> updates = getAvailableUpdates();
      if (updates.size()) {
        LOGGER_LOG(LVL_info, "got new updates, sending UpdatesReceived event");
        *events_channel << boost::shared_ptr<event::UpdatesReceived>(new event::UpdatesReceived(updates));
      } else {
        LOGGER_LOG(LVL_info, "no new updates, sending NoUpdateRequests event");
        *events_channel << boost::shared_ptr<event::NoUpdateRequests>(new event::NoUpdateRequests());
      }
    } else if (command->variant == "StartDownload") {
      command::StartDownload *start_download_command = command->toChild<command::StartDownload>();
      startDownload(start_download_command->update_request_id);
    } else if (command->variant == "SendUpdateReport") {
      command::SendUpdateReport *send_update_report_command = command->toChild<command::SendUpdateReport>();
      sendUpdateReport(send_update_report_command->update_report);
    }
  }
}
