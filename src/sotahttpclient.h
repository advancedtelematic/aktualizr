#ifndef SOTAHTTPCLIENT_H_
#define SOTAHTTPCLIENT_H_

#include <vector>

#include "config.h"
#include "httpclient.h"
#include "types.h"

class SotaHttpClient {
 public:
  SotaHttpClient(const Config &config_in);
  SotaHttpClient(const Config &config_in, HttpClient *http_in);
  ~SotaHttpClient();
  std::vector<data::UpdateRequest> getAvailableUpdates();
  Json::Value downloadUpdate(const data::UpdateRequestId &update_request_id);
  Json::Value reportUpdateResult(data::UpdateReport &update_report);

 private:
  Config config;
  HttpClient *http;
  std::string core_url;
};

#endif
