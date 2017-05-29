#ifndef HTTPCLIENT_H_
#define HTTPCLIENT_H_

#include <curl/curl.h>
#include "json/json.h"

#include "config.h"

class HttpClient {
 public:
  HttpClient();
  HttpClient(const HttpClient &);
  virtual ~HttpClient();
  virtual bool authenticate(const AuthConfig &conf);
  virtual bool authenticate(const std::string &cert, const std::string &ca_file, const std::string &pkey);
  virtual std::string get(const std::string &url);
  virtual Json::Value getJson(const std::string &url);
  virtual std::string post(const std::string &url, const std::string &data);
  virtual Json::Value post(const std::string &url, const Json::Value &data);
  virtual std::string put(const std::string &url, const std::string &data);

  virtual bool download(const std::string &url, const std::string &filename);
  bool isAuthenticated() { return authenticated; }
  void setCerts(const std::string &ca, const std::string &cert, const std::string &pkey);
  unsigned int http_code;
  std::string token; /**< the OAuth2 token stored as string */

 private:
  CURL *curl;
  curl_slist *headers;
  std::string perform(CURL *curl_handler);
  bool authenticated;
  std::string user_agent;
};
#endif
