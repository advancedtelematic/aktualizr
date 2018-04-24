#include "utilities/httpclient.h"
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

#include "openssl_compat.h"

/*****************************************************************************/
/**
 * \par Description:
 *    A writeback handler for the curl library. It handles writing response
 *    data from curl into a string.
 *    https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 */
static size_t writeString(void* contents, size_t size, size_t nmemb, void* userp) {
  assert(userp);
  // append the writeback data to the provided string
  (static_cast<std::string*>(userp))->append(static_cast<char*>(contents), size * nmemb);

  // return size of written data
  return size * nmemb;
}

HttpClient::HttpClient() : user_agent(std::string("Aktualizr/") + AKTUALIZR_VERSION) {
  curl = curl_easy_init();
  headers = nullptr;
  http_code = 0;

  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);

  // let curl use our write function
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

  curl_easy_setopt(curl, CURLOPT_VERBOSE, get_curlopt_verbose());

  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: */*");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
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

HttpClient::~HttpClient() {
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
}

HttpResponse HttpClient::get(const std::string& url) {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  LOG_DEBUG << "GET " << url;
  return perform(curl, RETRY_TIMES);
}

void HttpClient::setCerts(const std::string& ca, CryptoSource ca_source, const std::string& cert,
                          CryptoSource cert_source, const std::string& pkey, CryptoSource pkey_source) {
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
  curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

  if (ca_source == kPkcs11) {
    throw std::runtime_error("Accessing CA certificate on PKCS11 devices isn't currently supported");
  }
  std::unique_ptr<TemporaryFile> tmp_ca_file = std_::make_unique<TemporaryFile>("tls-ca");
  tmp_ca_file->PutContents(ca);
  curl_easy_setopt(curl, CURLOPT_CAINFO, tmp_ca_file->Path().c_str());
  tls_ca_file = std::move_if_noexcept(tmp_ca_file);

  if (cert_source == kPkcs11) {
    curl_easy_setopt(curl, CURLOPT_SSLCERT, cert.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "ENG");
  } else {  // cert_source == kFile
    std::unique_ptr<TemporaryFile> tmp_cert_file = std_::make_unique<TemporaryFile>("tls-cert");
    tmp_cert_file->PutContents(cert);
    curl_easy_setopt(curl, CURLOPT_SSLCERT, tmp_cert_file->Path().c_str());
    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    tls_cert_file = std::move_if_noexcept(tmp_cert_file);
  }
  pkcs11_cert = (cert_source == kPkcs11);

  if (pkey_source == kPkcs11) {
    curl_easy_setopt(curl, CURLOPT_SSLENGINE, "pkcs11");
    curl_easy_setopt(curl, CURLOPT_SSLENGINE_DEFAULT, 1L);
    curl_easy_setopt(curl, CURLOPT_SSLKEY, pkey.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "ENG");
  } else {  // pkey_source == kFile
    std::unique_ptr<TemporaryFile> tmp_pkey_file = std_::make_unique<TemporaryFile>("tls-pkey");
    tmp_pkey_file->PutContents(pkey);
    curl_easy_setopt(curl, CURLOPT_SSLKEY, tmp_pkey_file->Path().c_str());
    curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
    tls_pkey_file = std::move_if_noexcept(tmp_pkey_file);
  }
  pkcs11_key = (pkey_source == kPkcs11);
}

HttpResponse HttpClient::post(const std::string& url, const Json::Value& data) {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1);
  std::string data_str = Json::FastWriter().write(data);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_str.c_str());
  LOG_TRACE << "post request body:" << data;
  return perform(curl, RETRY_TIMES);
}

HttpResponse HttpClient::put(const std::string& url, const Json::Value& data) {
  CURL* curl_put = curl_easy_duphandle(curl);

  // TODO: it is a workaround for an unidentified bug in libcurl. Ideally the bug itself should be fixed.
  if (pkcs11_key) {
    curl_easy_setopt(curl_put, CURLOPT_SSLENGINE, "pkcs11");
    curl_easy_setopt(curl_put, CURLOPT_SSLKEYTYPE, "ENG");
  }

  if (pkcs11_cert) {
    curl_easy_setopt(curl_put, CURLOPT_SSLCERTTYPE, "ENG");
  }

  curl_easy_setopt(curl_put, CURLOPT_URL, url.c_str());
  std::string data_str = Json::FastWriter().write(data);
  curl_easy_setopt(curl_put, CURLOPT_POSTFIELDS, data_str.c_str());
  curl_easy_setopt(curl_put, CURLOPT_CUSTOMREQUEST, "PUT");
  LOG_TRACE << "put request body:" << data;
  HttpResponse result = perform(curl_put, RETRY_TIMES);
  curl_easy_cleanup(curl_put);
  return result;
}

HttpResponse HttpClient::perform(CURL* curl_handler, int retry_times) {
  std::string response_str;
  curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void*)&response_str);
  CURLcode result = curl_easy_perform(curl_handler);
  curl_easy_getinfo(curl_handler, CURLINFO_RESPONSE_CODE, &http_code);
  HttpResponse response(response_str, http_code, result, (result != CURLE_OK) ? curl_easy_strerror(result) : "");
  if (response.curl_code != CURLE_OK || response.http_status_code >= 500) {
    std::ostringstream error_message;
    error_message << "curl error " << response.curl_code << " (http code " << response.http_status_code
                  << "): " << response.error_message;
    LOG_ERROR << error_message.str();
    if (retry_times != 0) {
      sleep(1);
      response = perform(curl_handler, --retry_times);
    }
  }
  LOG_TRACE << "response: " << response.body;
  return response;
}

HttpResponse HttpClient::download(const std::string& url, curl_write_callback callback, void* userp) {
  CURL* curl_download = curl_easy_duphandle(curl);

  // TODO: it is a workaround for an unidentified bug in libcurl. Ideally the bug itself should be fixed.
  if (pkcs11_key) {
    curl_easy_setopt(curl_download, CURLOPT_SSLENGINE, "pkcs11");
    curl_easy_setopt(curl_download, CURLOPT_SSLKEYTYPE, "ENG");
  }

  if (pkcs11_cert) {
    curl_easy_setopt(curl_download, CURLOPT_SSLCERTTYPE, "ENG");
  }

  curl_easy_setopt(curl_download, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_download, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl_download, CURLOPT_WRITEFUNCTION, callback);
  curl_easy_setopt(curl_download, CURLOPT_WRITEDATA, userp);

  CURLcode result = curl_easy_perform(curl_download);
  curl_easy_getinfo(curl_download, CURLINFO_RESPONSE_CODE, &http_code);
  HttpResponse response("", http_code, result, (result != CURLE_OK) ? curl_easy_strerror(result) : "");
  curl_easy_cleanup(curl_download);
  return response;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
