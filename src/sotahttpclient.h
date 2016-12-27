#ifndef SOTAHTTPCLIENT_H_
#define SOTAHTTPCLIENT_H_

#include "config.h"
#include "httpclient.h"

class SotaHttpClient {
 public:
  SotaHttpClient(const Config &config_in);
  boost::property_tree::ptree getAvailableUpdates();
  boost::property_tree::ptree downloadUpdate(
      const boost::property_tree::ptree &pt);
  boost::property_tree::ptree reportUpdateResult(
      const boost::property_tree::ptree &download_status);

 private:
  HttpClient http;
  Config config;
  std::string core_url;
};

#endif