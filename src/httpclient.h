#ifndef HTTPCLIENT_H_
#define HTTPCLIENT_H_

#include <curl/curl.h>
#include "json/json.h"

#include "config.h"

class HttpClient {
 public:
  HttpClient();
  HttpClient(const HttpClient &);
  ~HttpClient();
  virtual bool authenticate(const AuthConfig &conf);
  virtual Json::Value get(const std::string &url);
  virtual Json::Value post(const std::string &url, const std::string &data);
  virtual Json::Value post(const std::string &url, const Json::Value &data);
  virtual bool download(const std::string &url, const std::string &filename);

 private:
  CURL *curl;
  curl_slist *headers;
  Json::Value perform(CURL *curl_handler);
  std::string token; /**< the OAuth2 token stored as string */
};
#endif
