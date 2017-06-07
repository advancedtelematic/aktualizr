#ifndef SOTAHTTPCLIENT_H_
#define SOTAHTTPCLIENT_H_

#include <vector>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "httpclient.h"
#include "types.h"

class SotaHttpClient {
 public:
  SotaHttpClient(const Config &config_in, event::Channel *events_channel_in);
  SotaHttpClient(const Config &config_in, HttpClient *http_in, event::Channel *events_channel_in);
  ~SotaHttpClient();
  std::vector<data::UpdateRequest> getAvailableUpdates();
  virtual void startDownload(const data::UpdateRequestId &update_request_id);
  virtual void startInstall(const data::UpdateRequestId &update_request_id);
  virtual void sendUpdateReport(data::UpdateReport update_report);
  bool authenticate();
  void run(command::Channel *commands_channel);
  void runForever(command::Channel *commands_channel);

 private:
  bool parseP12(FILE *p12_fp, const std::string &p12_password, const std::string &client_pem, const std::string ca_pem);
  Config config;
  HttpClient *http;
  event::Channel *events_channel;
  std::string core_url;
};

#endif
