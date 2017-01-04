#ifndef SOTAHTTPCLIENT_H_
#define SOTAHTTPCLIENT_H_

#include <vector>

#include "config.h"
#include "httpclient.h"
#include "types.h"

class SotaHttpClient {
 public:
  SotaHttpClient(const Config &config_in);
  std::vector<data::UpdateRequest> getAvailableUpdates();
  Json::Value downloadUpdate(const data::UpdateRequest &ur);
  Json::Value reportUpdateResult(const Json::Value &download_status);

 private:
  HttpClient http;
  Config config;
  std::string core_url;
};

#endif