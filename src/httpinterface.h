#ifndef HTTPINTERFACE_H_
#define HTTPINTERFACE_H_

#include <string>

#include <curl/curl.h>
#include "json/json.h"

#include "utils.h"

struct HttpResponse {
  HttpResponse(const std::string &body_in, unsigned int http_status_code_in, CURLcode curl_code_in,
               const std::string &error_message_in)
      : body(body_in),
        http_status_code(http_status_code_in),
        curl_code(curl_code_in),
        error_message(error_message_in) {}
  std::string body;
  unsigned int http_status_code;
  CURLcode curl_code;
  std::string error_message;
  bool isOk() { return (curl_code == CURLE_OK && http_status_code >= 200 && http_status_code < 205); }
  Json::Value getJson() { return Utils::parseJSON(body); }
};

class HttpInterface {
 public:
  HttpInterface(){};
  virtual ~HttpInterface(){};
  virtual bool authenticate(const std::string &cert, const std::string &ca_file, const std::string &pkey) = 0;
  virtual HttpResponse get(const std::string &url) = 0;
  virtual HttpResponse post(const std::string &url, const Json::Value &data) = 0;
  virtual HttpResponse put(const std::string &url, const Json::Value &data) = 0;

  virtual HttpResponse download(const std::string &url, curl_write_callback callback, void *userp) = 0;
  virtual void setCerts(const std::string &ca, const std::string &cert, const std::string &pkey) = 0;

 private:
  /**
   * These are here to catch a common programming error where a Json::Value is
   * implicitly constructed from a std::string. By having an private overload
   * that takes string (and with no implementation), this will fail during
   * compilation.
   */
  HttpResponse post(const std::string &url, std::string data);
  HttpResponse put(const std::string &url, std::string data);
};

#endif  // HTTPINTERFACE_H_
