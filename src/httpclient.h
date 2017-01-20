#ifndef HTTPCLIENT_H_
#define HTTPCLIENT_H_

#include <curl/curl.h>
#include <boost/noncopyable.hpp>
#include "json/json.h"

#include "config.h"

class HttpClient : private boost::noncopyable {
 public:
  HttpClient();
  ~HttpClient();
  bool authenticate(const AuthConfig &conf);
  Json::Value get(const std::string &url);
  Json::Value post(const std::string &url, const std::string &data);
  Json::Value post(const std::string &url, const Json::Value &data);
  bool download(const std::string &url, const std::string &filename);

 private:
  CURL *curl;
  curl_slist* headers;
  Json::Value perform(CURL *curl_handler);
  std::string token; /**< the OAuth2 token stored as string */
  std::string token_type;
};
#endif
