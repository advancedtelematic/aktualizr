#ifndef HTTPCLIENT_H_
#define HTTPCLIENT_H_

#include <curl/curl.h>
#include <boost/noncopyable.hpp>
#include <boost/property_tree/ptree.hpp>

#include "config.h"

class HttpClient : private boost::noncopyable {
 public:
  HttpClient();
  ~HttpClient();
  bool authenticate(const AuthConfig &conf);
  boost::property_tree::ptree get(const std::string &url);
  boost::property_tree::ptree post(const std::string &url,
                                   const std::string &data);
  bool download(const std::string &url, const std::string &filename);

 private:
  CURL *curl;
  boost::property_tree::ptree perform(CURL *curl_handler);
  std::string token; /**< the OAuth2 token stored as string */
  std::string token_type;
};
#endif
