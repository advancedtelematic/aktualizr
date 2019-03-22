#include "httpclient.h"
#include "utilities/utils.h"

#include <assert.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

#include "crypto/openssl_compat.h"
#include "utilities/utils.h"

struct WriteStringArg {
  std::string out;
  int64_t limit{0};
};

/*****************************************************************************/
/**
 * \par Description:
 *    A writeback handler for the curl library. It handles writing response
 *    data from curl into a string.
 *    https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 */
static size_t writeString(void* contents, size_t size, size_t nmemb, void* userp) {
  assert(contents);
  assert(userp);
  // append the writeback data to the provided string
  auto* arg = static_cast<WriteStringArg*>(userp);
  if (arg->limit > 0) {
    if (arg->out.length() + size * nmemb > static_cast<uint64_t>(arg->limit)) {
      return 0;
    }
  }
  (static_cast<WriteStringArg*>(userp))->out.append(static_cast<char*>(contents), size * nmemb);

  // return size of written data
  return size * nmemb;
}

HttpClient::HttpClient() : user_agent(std::string("Aktualizr/") + AKTUALIZR_VERSION) {
  curl = curl_easy_init();
  if (curl == nullptr) {
    throw std::runtime_error("Could not initialize curl");
  }
  headers = nullptr;

  curlEasySetoptWrapper(curl, CURLOPT_NOSIGNAL, 1L);
  curlEasySetoptWrapper(curl, CURLOPT_TIMEOUT, 60L);
  curlEasySetoptWrapper(curl, CURLOPT_CONNECTTIMEOUT, 60L);

  // let curl use our write function
  curlEasySetoptWrapper(curl, CURLOPT_WRITEFUNCTION, writeString);
  curlEasySetoptWrapper(curl, CURLOPT_WRITEDATA, NULL);

  curlEasySetoptWrapper(curl, CURLOPT_VERBOSE, get_curlopt_verbose());

  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: */*");
  curlEasySetoptWrapper(curl, CURLOPT_HTTPHEADER, headers);
  curlEasySetoptWrapper(curl, CURLOPT_USERAGENT, user_agent.c_str());
}

HttpClient::HttpClient(const HttpClient& curl_in) : pkcs11_key(curl_in.pkcs11_key), pkcs11_cert(curl_in.pkcs11_key) {
  curl = curl_easy_duphandle(curl_in.curl);

  struct curl_slist* inlist = curl_in.headers;
  headers = nullptr;
  struct curl_slist* tmp;

  while (inlist != nullptr) {
    tmp = curl_slist_append(headers, inlist->data);

    if (tmp == nullptr) {
      curl_slist_free_all(headers);
      throw std::runtime_error("curl_slist_append returned null");
    }

    headers = tmp;
    inlist = inlist->next;
  }
}

CurlGlobalInitWrapper HttpClient::manageCurlGlobalInit_{};

HttpClient::~HttpClient() {
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
}

HttpResponse HttpClient::get(const std::string& url, int64_t maxsize) {
  CURL* curl_get = Utils::curlDupHandleWrapper(curl, pkcs11_key);

  // TODO: it is a workaround for an unidentified bug in libcurl. Ideally the bug itself should be fixed.
  if (pkcs11_key) {
    curlEasySetoptWrapper(curl_get, CURLOPT_SSLENGINE, "pkcs11");
    curlEasySetoptWrapper(curl_get, CURLOPT_SSLKEYTYPE, "ENG");
  }

  if (pkcs11_cert) {
    curlEasySetoptWrapper(curl_get, CURLOPT_SSLCERTTYPE, "ENG");
  }

  // Clear POSTFIELDS to remove any lingering references to strings that have
  // probably since been deallocated.
  curlEasySetoptWrapper(curl_get, CURLOPT_POSTFIELDS, "");
  curlEasySetoptWrapper(curl_get, CURLOPT_URL, url.c_str());
  curlEasySetoptWrapper(curl_get, CURLOPT_HTTPGET, 1L);
  if (maxsize >= 0) {
    // it will only take effect if the server declares the size in advance,
    //    writeString callback takes care of the other case
    curlEasySetoptWrapper(curl_get, CURLOPT_MAXFILESIZE_LARGE, maxsize);
  }
  curlEasySetoptWrapper(curl_get, CURLOPT_LOW_SPEED_TIME, speed_limit_time_interval_);
  curlEasySetoptWrapper(curl_get, CURLOPT_LOW_SPEED_LIMIT, speed_limit_bytes_per_sec_);
  LOG_DEBUG << "GET " << url;
  HttpResponse response = perform(curl_get, RETRY_TIMES, maxsize);
  curl_easy_cleanup(curl_get);
  return response;
}

void HttpClient::setCerts(const std::string& ca, CryptoSource ca_source, const std::string& cert,
                          CryptoSource cert_source, const std::string& pkey, CryptoSource pkey_source) {
  curlEasySetoptWrapper(curl, CURLOPT_SSL_VERIFYPEER, 1);
  curlEasySetoptWrapper(curl, CURLOPT_SSL_VERIFYHOST, 2);
  curlEasySetoptWrapper(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

  if (ca_source == CryptoSource::kPkcs11) {
    throw std::runtime_error("Accessing CA certificate on PKCS11 devices isn't currently supported");
  }
  std::unique_ptr<TemporaryFile> tmp_ca_file = std_::make_unique<TemporaryFile>("tls-ca");
  tmp_ca_file->PutContents(ca);
  curlEasySetoptWrapper(curl, CURLOPT_CAINFO, tmp_ca_file->Path().c_str());
  tls_ca_file = std::move_if_noexcept(tmp_ca_file);

  if (cert_source == CryptoSource::kPkcs11) {
    curlEasySetoptWrapper(curl, CURLOPT_SSLCERT, cert.c_str());
    curlEasySetoptWrapper(curl, CURLOPT_SSLCERTTYPE, "ENG");
  } else {  // cert_source == CryptoSource::kFile
    std::unique_ptr<TemporaryFile> tmp_cert_file = std_::make_unique<TemporaryFile>("tls-cert");
    tmp_cert_file->PutContents(cert);
    curlEasySetoptWrapper(curl, CURLOPT_SSLCERT, tmp_cert_file->Path().c_str());
    curlEasySetoptWrapper(curl, CURLOPT_SSLCERTTYPE, "PEM");
    tls_cert_file = std::move_if_noexcept(tmp_cert_file);
  }
  pkcs11_cert = (cert_source == CryptoSource::kPkcs11);

  if (pkey_source == CryptoSource::kPkcs11) {
    curlEasySetoptWrapper(curl, CURLOPT_SSLENGINE, "pkcs11");
    curlEasySetoptWrapper(curl, CURLOPT_SSLENGINE_DEFAULT, 1L);
    curlEasySetoptWrapper(curl, CURLOPT_SSLKEY, pkey.c_str());
    curlEasySetoptWrapper(curl, CURLOPT_SSLKEYTYPE, "ENG");
  } else {  // pkey_source == CryptoSource::kFile
    std::unique_ptr<TemporaryFile> tmp_pkey_file = std_::make_unique<TemporaryFile>("tls-pkey");
    tmp_pkey_file->PutContents(pkey);
    curlEasySetoptWrapper(curl, CURLOPT_SSLKEY, tmp_pkey_file->Path().c_str());
    curlEasySetoptWrapper(curl, CURLOPT_SSLKEYTYPE, "PEM");
    tls_pkey_file = std::move_if_noexcept(tmp_pkey_file);
  }
  pkcs11_key = (pkey_source == CryptoSource::kPkcs11);
}

HttpResponse HttpClient::post(const std::string& url, const Json::Value& data) {
  CURL* curl_post = Utils::curlDupHandleWrapper(curl, pkcs11_key);
  curlEasySetoptWrapper(curl_post, CURLOPT_URL, url.c_str());
  curlEasySetoptWrapper(curl_post, CURLOPT_POST, 1);
  std::string data_str = Json::FastWriter().write(data);
  curlEasySetoptWrapper(curl_post, CURLOPT_POSTFIELDS, data_str.c_str());
  LOG_TRACE << "post request body:" << data;
  auto result = perform(curl_post, RETRY_TIMES, HttpInterface::kPostRespLimit);
  curl_easy_cleanup(curl_post);
  return result;
}

HttpResponse HttpClient::put(const std::string& url, const Json::Value& data) {
  CURL* curl_put = Utils::curlDupHandleWrapper(curl, pkcs11_key);
  curlEasySetoptWrapper(curl_put, CURLOPT_URL, url.c_str());
  std::string data_str = Json::FastWriter().write(data);
  curlEasySetoptWrapper(curl_put, CURLOPT_POSTFIELDS, data_str.c_str());
  curlEasySetoptWrapper(curl_put, CURLOPT_CUSTOMREQUEST, "PUT");
  LOG_TRACE << "put request body:" << data;
  HttpResponse result = perform(curl_put, RETRY_TIMES, HttpInterface::kPutRespLimit);
  curl_easy_cleanup(curl_put);
  return result;
}

HttpResponse HttpClient::perform(CURL* curl_handler, int retry_times, int64_t size_limit) {
  WriteStringArg response_arg;
  response_arg.limit = size_limit;
  curlEasySetoptWrapper(curl_handler, CURLOPT_WRITEDATA, static_cast<void*>(&response_arg));
  CURLcode result = curl_easy_perform(curl_handler);
  long http_code;  // NOLINT(google-runtime-int)
  curl_easy_getinfo(curl_handler, CURLINFO_RESPONSE_CODE, &http_code);
  HttpResponse response(response_arg.out, http_code, result, (result != CURLE_OK) ? curl_easy_strerror(result) : "");
  if (response.curl_code != CURLE_OK || response.http_status_code >= 500) {
    std::ostringstream error_message;
    error_message << "curl error " << response.curl_code << " (http code " << response.http_status_code
                  << "): " << response.error_message;
    LOG_ERROR << error_message.str();
    if (retry_times != 0) {
      sleep(1);
      response = perform(curl_handler, --retry_times, size_limit);
    }
  }
  LOG_TRACE << "response http code: " << response.http_status_code;
  LOG_TRACE << "response: " << response.body;
  return response;
}

HttpResponse HttpClient::download(const std::string& url, curl_write_callback write_cb,
                                  curl_xferinfo_callback progress_cb, void* userp, curl_off_t from) {
  return downloadAsync(url, write_cb, progress_cb, userp, from, nullptr).get();
}

std::future<HttpResponse> HttpClient::downloadAsync(const std::string& url, curl_write_callback write_cb,
                                                    curl_xferinfo_callback progress_cb, void* userp, curl_off_t from,
                                                    CurlHandler* easyp) {
  CURL* curl_download = Utils::curlDupHandleWrapper(curl, pkcs11_key);

  CurlHandler curlp = CurlHandler(curl_download, curl_easy_cleanup);

  if (easyp != nullptr) {
    *easyp = curlp;
  }

  curlEasySetoptWrapper(curl_download, CURLOPT_URL, url.c_str());
  curlEasySetoptWrapper(curl_download, CURLOPT_HTTPGET, 1L);
  curlEasySetoptWrapper(curl_download, CURLOPT_FOLLOWLOCATION, 1L);
  curlEasySetoptWrapper(curl_download, CURLOPT_WRITEFUNCTION, write_cb);
  curlEasySetoptWrapper(curl_download, CURLOPT_WRITEDATA, userp);
  if (progress_cb != nullptr) {
    curlEasySetoptWrapper(curl_download, CURLOPT_NOPROGRESS, 0);
    curlEasySetoptWrapper(curl_download, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curlEasySetoptWrapper(curl_download, CURLOPT_XFERINFODATA, userp);
  }
  curlEasySetoptWrapper(curl_download, CURLOPT_TIMEOUT, 0);
  curlEasySetoptWrapper(curl_download, CURLOPT_LOW_SPEED_TIME, speed_limit_time_interval_);
  curlEasySetoptWrapper(curl_download, CURLOPT_LOW_SPEED_LIMIT, speed_limit_bytes_per_sec_);
  curlEasySetoptWrapper(curl_download, CURLOPT_RESUME_FROM_LARGE, from);

  std::promise<HttpResponse> resp_promise;
  auto resp_future = resp_promise.get_future();
  std::thread(
      [curlp](std::promise<HttpResponse> promise) {
        CURLcode result = curl_easy_perform(curlp.get());
        long http_code;  // NOLINT(google-runtime-int)
        curl_easy_getinfo(curlp.get(), CURLINFO_RESPONSE_CODE, &http_code);
        HttpResponse response("", http_code, result, (result != CURLE_OK) ? curl_easy_strerror(result) : "");
        promise.set_value(response);
      },
      std::move(resp_promise))
      .detach();
  return resp_future;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
