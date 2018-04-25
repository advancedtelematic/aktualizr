#ifndef HTTPINTERFACE_H_
#define HTTPINTERFACE_H_

#include <string>
#include <utility>

#include <curl/curl.h>
#include "json/json.h"

#include "utilities/types.h"
#include "utilities/utils.h"

struct HttpResponse {
  HttpResponse(std::string body_in, const long http_status_code_in, CURLcode curl_code_in,  // NOLINT
               std::string error_message_in)
      : body(std::move(body_in)),
        http_status_code(http_status_code_in),
        curl_code(curl_code_in),
        error_message(std::move(error_message_in)) {}
  std::string body;
  long http_status_code;  // NOLINT
  CURLcode curl_code;
  std::string error_message;
  bool isOk() { return (curl_code == CURLE_OK && http_status_code >= 200 && http_status_code < 205); }
  Json::Value getJson() { return Utils::parseJSON(body); }
};

class HttpInterface {
 public:
  HttpInterface() = default;
  virtual ~HttpInterface() = default;
  virtual HttpResponse get(const std::string &url) = 0;
  virtual HttpResponse post(const std::string &url, const Json::Value &data) = 0;
  virtual HttpResponse put(const std::string &url, const Json::Value &data) = 0;

  virtual HttpResponse download(const std::string &url, curl_write_callback callback, void *userp) = 0;
  virtual void setCerts(const std::string &ca, CryptoSource ca_source, const std::string &cert,
                        CryptoSource cert_source, const std::string &pkey, CryptoSource pkey_source) = 0;
};

#endif  // HTTPINTERFACE_H_
