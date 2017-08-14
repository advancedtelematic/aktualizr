#ifndef HTTPCLIENT_H_
#define HTTPCLIENT_H_

#include <curl/curl.h>
#include <boost/move/unique_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include "json/json.h"

#include "config.h"
#include "httpinterface.h"
#include "utils.h"

/**
 * Helper class to manage curl_global_init/curl_global_cleanup calls
 */
class CurlGlobalInitWrapper {
 public:
  CurlGlobalInitWrapper() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlGlobalInitWrapper() { curl_global_cleanup(); }
};

class HttpClient : public HttpInterface {
 public:
  HttpClient();
  HttpClient(const HttpClient &);
  virtual ~HttpClient();
  virtual bool authenticate(const std::string &cert, const std::string &ca_file, const std::string &pkey);
  virtual HttpResponse get(const std::string &url);
  virtual HttpResponse post(const std::string &url, const Json::Value &data);
  virtual HttpResponse put(const std::string &url, const Json::Value &data);

  virtual HttpResponse download(const std::string &url, curl_write_callback callback, void *userp);
  virtual void setCerts(const std::string &ca, const std::string &cert, const std::string &pkey);
  unsigned int http_code;
  std::string token; /**< the OAuth2 token stored as string */

 private:
  CurlGlobalInitWrapper manageCurlGlobalInit_;  // Must be first member to ensure curl init/shutdown happens first/last
  CURL *curl;
  curl_slist *headers;
  HttpResponse perform(CURL *curl_handler, int retry_times);
  std::string user_agent;

  static CURLcode sslCtxFunction(CURL *handle, void *sslctx, void *parm);
  boost::mutex tls_mutex;
  boost::movelib::unique_ptr<TemporaryFile> tls_ca_file;
  boost::movelib::unique_ptr<TemporaryFile> tls_cert_file;
  boost::movelib::unique_ptr<TemporaryFile> tls_pkey_file;
  static const int RETRY_TIMES = 2;
};
#endif
