#include "sotarviclient.h"

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include <stdexcept>

#include "logger.h"
#include "types.h"

Json::Value parseParameters(const char *parameters) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(parameters, json);
  return json;
}

void notifyCallback(int fd, void *service_data, const char *parameters) {
  (void)fd;
  LOGGER_LOG(LVL_info, "got callback notify");
  ((SotaRVIClient *)service_data)
      ->sendEvent(boost::make_shared<event::UpdateAvailable>(
          event::UpdateAvailable(data::UpdateAvailable::fromJson(parseParameters(parameters)[0]["update_available"]))));
}

void startCallback(int fd, void *service_data, const char *parameters) {
  (void)fd;
  LOGGER_LOG(LVL_info, "got callback start");
  ((SotaRVIClient *)service_data)->invokeAck(parseParameters(parameters)[0]["update_id"].asString());
}

void chunkCallback(int fd, void *service_data, const char *parameters) {
  (void)fd;
  LOGGER_LOG(LVL_info, "got callback chunk");
  ((SotaRVIClient *)service_data)->saveChunk(parseParameters(parameters)[0]);
}

void finishCallback(int fd, void *service_data, const char *parameters) {
  (void)fd;
  LOGGER_LOG(LVL_info, "got callback finish");
  ((SotaRVIClient *)service_data)->downloadComplete(parseParameters(parameters)[0]);
}

void getpackagesCallback(int fd, void *service_data, const char *parameters) {
  (void)fd;
  (void)parameters;
  LOGGER_LOG(LVL_info, "got callback getpackages");
  ((SotaRVIClient *)service_data)->sendEvent(boost::make_shared<event::InstalledSoftwareNeeded>());
}

SotaRVIClient::SotaRVIClient(const Config &config_in, event::Channel *events_channel_in)
    : config(config_in), events_channel(events_channel_in), stop(false) {
  Json::Value json;
  json["dev"]["key"] = config.rvi.device_key;
  json["dev"]["cert"] = config.rvi.device_cert;
  json["dev"]["id"] = std::string("genivi.org/device/") + config.device.uuid;
  json["ca"]["cert"] = config.rvi.ca_cert;
  json["ca"]["dir"] = config.rvi.cert_dir;
  json["creddir"] = config.rvi.cred_dir;

  rviSetVerboseLogs(loggerGetSeverity() == LVL_trace);

  rvi = rviJsonInit(const_cast<char *>(Json::FastWriter().write(json).c_str()));
  if (!rvi) {
    throw std::runtime_error("cannot initialize rvi with config " + Json::FastWriter().write(json));
  }

  connection = rviConnect(rvi, config.rvi.node_host.c_str(), config.rvi.node_port.c_str());

  if (connection <= 0) {
    LOGGER_LOG(LVL_error, "cannot connect to rvi node");
  }

  std::string service_name = "notify";
  int stat = rviRegisterService(rvi, (std::string("sota/") + service_name).c_str(), notifyCallback, (void *)this);
  if (stat) {
    LOGGER_LOG(LVL_error, "unable to register " << service_name);
  }

  service_name = "start";
  stat = rviRegisterService(rvi, (std::string("sota/") + service_name).c_str(), startCallback, (void *)this);
  if (stat) {
    LOGGER_LOG(LVL_error, "unable to register " << service_name);
  }

  service_name = "chunk";
  stat = rviRegisterService(rvi, (std::string("sota/") + service_name).c_str(), chunkCallback, (void *)this);
  if (stat) {
    LOGGER_LOG(LVL_error, "unable to register " << service_name);
  }

  service_name = "finish";
  stat = rviRegisterService(rvi, (std::string("sota/") + service_name).c_str(), finishCallback, (void *)this);
  if (stat) {
    LOGGER_LOG(LVL_error, "unable to register " << service_name);
  }

  service_name = "getpackages";
  stat = rviRegisterService(rvi, (std::string("sota/") + service_name).c_str(), getpackagesCallback, (void *)this);
  if (stat) {
    LOGGER_LOG(LVL_error, "unable to register " << service_name);
  }

  LOGGER_LOG(LVL_info, "rvi services registered!!");
  thread = boost::thread(boost::bind(&SotaRVIClient::run, this));
}

SotaRVIClient::~SotaRVIClient() {
  rviCleanup(rvi);
  stop = true;
  if (!thread.try_join_for(boost::chrono::seconds(10))) {
    LOGGER_LOG(LVL_error, "join()-ing DBusGateway thread timed out");
  }
}

void SotaRVIClient::sendEvent(const boost::shared_ptr<event::BaseEvent> &event) { *events_channel << event; }

void SotaRVIClient::run() {
  while (true) {
    int result = rviProcessInput(rvi, &connection, 1);
    if (stop) {
      break;
    }
    switch (result) {
      case RVI_OK:
        break;
      case RVI_ERR_JSON_PART:
        LOGGER_LOG(LVL_trace, "Partial RVI message, continuing");
        break;
      default: LOGGER_LOG(LVL_error, "rviProcessInput error: " << result);
        sleep(5);
    }
  }
}

void SotaRVIClient::startDownload(const data::UpdateRequestId &update_id) {
  Json::Value value;
  value["device"] = config.device.uuid;
  value["update_id"] = update_id;
  value["services"] = services;
  Json::Value params(Json::arrayValue);
  params.append(value);
  int stat = rviInvokeService(rvi, "genivi.org/backend/sota/start", Json::FastWriter().write(params).c_str());
  LOGGER_LOG(LVL_info, "invoked genivi.org/backend/sota/start, return code is " << stat);
}

void SotaRVIClient::invokeAck(const std::string &update_id) {
  std::vector<int> chunks = chunks_map[update_id];

  Json::Value value;
  value["device"] = config.device.uuid;
  value["update_id"] = update_id;
  value["chunks"] = Json::arrayValue;

  for (std::vector<int>::iterator it = chunks.begin(); it != chunks.end(); ++it) {
    value["chunks"].append(*it);
  }
  Json::Value params(Json::arrayValue);
  params.append(value);
  int stat = rviInvokeService(rvi, "genivi.org/backend/sota/ack", Json::FastWriter().write(params).c_str());
  LOGGER_LOG(LVL_info, "invoked genivi.org/backend/sota/ack, return code is " << stat);
}

void SotaRVIClient::saveChunk(const Json::Value &chunk_json) {
  std::string b64_text = chunk_json["bytes"].asString();

  unsigned long long paddChars = std::count(b64_text.begin(), b64_text.end(), '=');
  std::replace(b64_text.begin(), b64_text.end(), '=', 'A');
  std::string output(base64_to_bin(b64_text.begin()), base64_to_bin(b64_text.end()));
  output.erase(output.end() - static_cast<unsigned int>(paddChars), output.end());

  std::ofstream update_file((config.device.packages_dir / chunk_json["update_id"].asString()).string().c_str(),
                            std::ios::out | std::ios::app | std::ios::binary);
  update_file << output;
  update_file.close();

  chunks_map[chunk_json["update_id"].asString()].push_back(chunk_json["index"].asInt());
  invokeAck(chunk_json["update_id"].asString());
}

void SotaRVIClient::downloadComplete(const Json::Value &parameters) {
  data::DownloadComplete download_complete;
  download_complete.update_id = parameters["update_id"].asString();
  download_complete.signature = parameters["signature"].asString();
  download_complete.update_image = (config.device.packages_dir / download_complete.update_id).string();
  sendEvent(boost::make_shared<event::DownloadComplete>(download_complete));
}

void SotaRVIClient::sendUpdateReport(data::UpdateReport update_report) {
  Json::Value params(Json::arrayValue);
  Json::Value report;
  report["device"] = config.device.uuid;
  report["update_report"] = update_report.toJson();
  params.append(report);

  int stat = rviInvokeService(rvi, "genivi.org/backend/sota/report", Json::FastWriter().write(params).c_str());
  LOGGER_LOG(LVL_info, "invoked genivi.org/backend/sota/report, return code is " << stat);
  chunks_map.erase(update_report.update_id);
}

void SotaRVIClient::runForever(command::Channel *commands_channel) {
  boost::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    LOGGER_LOG(LVL_info, "got " + command->variant + " command");
    if (command->variant == "StartDownload") {
      command::StartDownload *start_download_command = command->toChild<command::StartDownload>();
      startDownload(start_download_command->update_request_id);
    } else if (command->variant == "SendUpdateReport") {
      command::SendUpdateReport *send_update_report_command = command->toChild<command::SendUpdateReport>();
      sendUpdateReport(send_update_report_command->update_report);
    }
  }
}
