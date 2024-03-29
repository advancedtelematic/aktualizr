#ifndef HTTPCLIENT_H_
#define HTTPCLIENT_H_

#include <future>
#include <memory>

#include <curl/curl.h>
#include <tuple>
#include "gtest/gtest_prod.h"
#include "json/json.h"

#include "httpinterface.h"
#include "logging/logging.h"
#include "utilities/utils.h"

/**
 * Helper class to manage curl_global_init/curl_global_cleanup calls
 */
class CurlGlobalInitWrapper {
 public:
  CurlGlobalInitWrapper() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlGlobalInitWrapper() { curl_global_cleanup(); }
  CurlGlobalInitWrapper &operator=(const CurlGlobalInitWrapper &) = delete;
  CurlGlobalInitWrapper(const CurlGlobalInitWrapper &) = delete;
  CurlGlobalInitWrapper &operator=(CurlGlobalInitWrapper &&) = delete;
  CurlGlobalInitWrapper(CurlGlobalInitWrapper &&) = delete;
};

class HttpClient : public HttpInterface {
 public:
  HttpClient(const std::vector<std::string> *extra_headers = nullptr);
  HttpClient(const HttpClient & /*curl_in*/);
  ~HttpClient() override;
  HttpResponse get(const std::string &url, int64_t maxsize) override;
  HttpResponse post(const std::string &url, const std::string &content_type, const std::string &data) override;
  HttpResponse post(const std::string &url, const Json::Value &data) override;
  HttpResponse put(const std::string &url, const std::string &content_type, const std::string &data) override;
  HttpResponse put(const std::string &url, const Json::Value &data) override;

  HttpResponse download(const std::string &url, curl_write_callback write_cb, curl_xferinfo_callback progress_cb,
                        void *userp, curl_off_t from) override;
  std::future<HttpResponse> downloadAsync(const std::string &url, curl_write_callback write_cb,
                                          curl_xferinfo_callback progress_cb, void *userp, curl_off_t from,
                                          CurlHandler *easyp) override;
  void setCerts(const std::string &ca, CryptoSource ca_source, const std::string &cert, CryptoSource cert_source,
                const std::string &pkey, CryptoSource pkey_source) override;
  bool updateHeader(const std::string &name, const std::string &value);

  /**
   * @brief use proxy_url as CURLOPT_PROXY parameter
   *
   * @param proxy_url Proxy and port, scheme is mandatory. Passing it to libcurl as CURLOPT_PROXY
   */
  void setProxy(const std::string &proxy_url) override;
  void setProxyCredentials(const std::string &username, const std::string &password) override;
  void setUseOscpStapling(bool oscp) override;

  /**
   * @brief If a download exceeds this maxspeed (counted in bytes per second) the transfer will
   * pause to keep the speed less than or equal to the parameter value. Defaults to unlimited speed.
   *
   * @param maxspeed counted in bytes per second
   */
  void setBandwidth(int64_t maxspeed) override;

  void reset() override;

 private:
  FRIEND_TEST(GetTest, download_speed_limit);

  static CurlGlobalInitWrapper manageCurlGlobalInit_;
  CURL *curl;
  curl_slist *headers;
  HttpResponse perform(CURL *curl_handler, int retry_times, int64_t size_limit);
  void setOptProxy(CURL *curl_handler);
  static curl_slist *curl_slist_dup(curl_slist *sl);

  static CURLcode sslCtxFunction(CURL *handle, void *sslctx, void *parm);
  std::unique_ptr<TemporaryFile> tls_ca_file;
  std::unique_ptr<TemporaryFile> tls_cert_file;
  std::unique_ptr<TemporaryFile> tls_pkey_file;
  static const int RETRY_TIMES = 2;
  static const long kSpeedLimitTimeInterval = 60L;   // NOLINT(google-runtime-int)
  static const long kSpeedLimitBytesPerSec = 5000L;  // NOLINT(google-runtime-int)

  long speed_limit_time_interval_{kSpeedLimitTimeInterval};                // NOLINT(google-runtime-int)
  long speed_limit_bytes_per_sec_{kSpeedLimitBytesPerSec};                 // NOLINT(google-runtime-int)
  void overrideSpeedLimitParams(long time_interval, long bytes_per_sec) {  // NOLINT(google-runtime-int)
    speed_limit_time_interval_ = time_interval;
    speed_limit_bytes_per_sec_ = bytes_per_sec;
  }
  bool pkcs11_key{false};
  bool pkcs11_cert{false};
  std::string proxy;
  std::string proxy_user;
  std::string proxy_pwd;
  bool oscp_stapling{false};
  curl_off_t bandwidth{0};  // 0 means "no limitations",  counted in bytes per second

  CurlHandler curlp;                // handler for current downloadAsync(), keep it to reset current download
  std::atomic<bool> reset_{false};  // use to detect reset during downloadAsync()
  // save cert configuration from last setCerts() call and use it for reset
  std::tuple<bool, std::string, CryptoSource, std::string, CryptoSource, std::string, CryptoSource> certs_in_use;
};
#endif
