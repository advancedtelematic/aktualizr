#include "sotarviclient.h"

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include <stdexcept>

#include "logger.h"
#include "types.h"

char service_names[6][12] = {"notify", "start", "chunk", "finish", "getpackages", "abort"};

void callbackWrapper(int fd, void *service_data, const char *parameters) {
  (void)fd;
  SotaRVIClient *client = (SotaRVIClient *)((void **)service_data)[0];
  std::string service((char *)((void **)service_data)[1]);
  Json::Reader reader;
  Json::Value json;
  reader.parse(parameters, json);

  std::cout << "got callback " << service << "\n";
  if (service == "notify") {
    client->sendEvent(boost::make_shared<event::UpdateAvailable>(
        event::UpdateAvailable(data::UpdateAvailable::fromJson(json[0]["update_available"]))));
  } else if (service == "start") {
    client->invokeAck(json[0]["update_id"].asString());
  } else if (service == "chunk") {
    client->saveChunk(json[0]);
  } else if (service == "finish") {
    client->downloadComplete(json[0]);
  } else if (service == "getpackages") {
    LOGGER_LOG(LVL_error, "service " << service << "not implemented yet");
  }
}

SotaRVIClient::SotaRVIClient(const Config &config_in, event::Channel *events_channel_in)
    : config(config_in), events_channel(events_channel_in) {
  std::string client_config("./config/" + config.rvi.client_config);
  rvi = rviInitLogs(const_cast<char *>(client_config.c_str()), !loggerGetSeverity());
  if (!rvi) {
    throw std::runtime_error("cannot initialize rvi with config file " + client_config);
  }
  connection = rviConnect(rvi, config.rvi.node_host.c_str(), config.rvi.node_port.c_str());

  if (connection <= 0) {
    LOGGER_LOG(LVL_error, "cannot connect to rvi node");
  }
  unsigned int services_count = sizeof(service_names) / sizeof(service_names[0]);
  void *pointers[2];
  pointers[0] = (void *)(this);
  for (unsigned int i = 0; i < services_count; ++i) {
    pointers[1] = (void *)service_names[i];
    int stat = rviRegisterService(rvi, (std::string("sota/") + service_names[i]).c_str(), callbackWrapper, pointers,
                                  sizeof(pointers));
    if (stat) {
      LOGGER_LOG(LVL_error, "unable to register " << service_names[i]);
    }
    services[service_names[i]] = std::string("genivi.org/" + config.device.uuid + "/") + service_names[i];
  }
  LOGGER_LOG(LVL_info, "rvi services registered!!");
  boost::thread(boost::bind(&SotaRVIClient::run, this));
}

void SotaRVIClient::sendEvent(const boost::shared_ptr<event::BaseEvent> &event) { *events_channel << event; }

void SotaRVIClient::run() {
  while (true) {
    int result = rviProcessInput(rvi, &connection, 1);
    if (result != 0) {
      LOGGER_LOG(LVL_error, "rviProcessInput error: " << result);
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

  std::ofstream update_file(config.device.packages_dir + chunk_json["update_id"].asString(),
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
  download_complete.update_image = config.device.packages_dir + download_complete.update_id;
  sendEvent(boost::make_shared<event::DownloadComplete>(download_complete));
}

void SotaRVIClient::sendUpdateReport(data::UpdateReport update_report) {
  Json::Value params(Json::arrayValue);
  params.append(update_report.toJson());

  int stat = rviInvokeService(rvi, "genivi.org/backend/sota/report", Json::FastWriter().write(params).c_str());
  LOGGER_LOG(LVL_info, "invoked genivi.org/backend/sota/report, return code is " << stat);
  chunks_map.erase(update_report.update_id);
}
