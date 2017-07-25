#include "httpclient.h"

#include <assert.h>
#include <sys/stat.h>

#include "logger.h"

/*****************************************************************************/
/**
 * \par Description:
 *    A writeback handler for the curl library. I handles writing response
 *    data from curl into a string.
 *    https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 */
static size_t writeString(void* contents, size_t size, size_t nmemb, void* userp) {
  assert(userp);
  // append the writeback data to the provided string
  (static_cast<std::string*>(userp))->append((char*)contents, size * nmemb);

  // return size of written data
  return size * nmemb;
}

static size_t writeFile(void* contents, size_t size, size_t nmemb, FILE* fp) {
  size_t written = fwrite(contents, size, nmemb, fp);
  return written;
}

// Discard the http body
static size_t writeDiscard(void*, size_t size, size_t nmemb) { return size * nmemb; }

HttpClient::HttpClient() : user_agent(std::string("Aktualizr/") + AKTUALIZR_VERSION) {
  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  headers = NULL;
  http_code = 0;

  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);

  // let curl use our write function
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

  if (loggerGetSeverity() == LVL_trace) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }

  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: */*");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
}

HttpClient::HttpClient(const HttpClient& curl_in) {
  curl = curl_easy_duphandle(curl_in.curl);
  token = curl_in.token;

  struct curl_slist* inlist = curl_in.headers;
  headers = NULL;
  struct curl_slist* tmp;

  while (inlist) {
    tmp = curl_slist_append(headers, inlist->data);

    if (!tmp) {
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

bool HttpClient::authenticate(const std::string& cert, const std::string& ca_file, const std::string& pkey) {
  // TODO return false in case of wrong certificates
  setCerts(ca_file, cert, pkey);
  return true;
}

HttpResponse HttpClient::get(const std::string& url) {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  LOGGER_LOG(LVL_debug, "GET " << url);
  return perform(curl, RETRY_TIMES);
}

void HttpClient::setCerts(const std::string& ca, const std::string& cert, const std::string& pkey) {
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, true);
  curl_easy_setopt(curl, CURLOPT_CAINFO, ca.c_str());
  curl_easy_setopt(curl, CURLOPT_SSLCERT, cert.c_str());
  curl_easy_setopt(curl, CURLOPT_SSLKEY, pkey.c_str());
}

HttpResponse HttpClient::post(const std::string& url, const Json::Value& data) {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1);
  std::string data_str = Json::FastWriter().write(data);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_str.c_str());
  LOGGER_LOG(LVL_trace, "post request body:" << data);
  return perform(curl, RETRY_TIMES);
}

HttpResponse HttpClient::put(const std::string& url, const Json::Value& data) {
  CURL* curl_put = curl_easy_duphandle(curl);

  curl_easy_setopt(curl_put, CURLOPT_URL, url.c_str());
  std::string data_str = Json::FastWriter().write(data);
  curl_easy_setopt(curl_put, CURLOPT_POSTFIELDS, data_str.c_str());
  curl_easy_setopt(curl_put, CURLOPT_CUSTOMREQUEST, "PUT");
  LOGGER_LOG(LVL_trace, "put request body:" << data);
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
  if (!response.isOk()) {
    std::ostringstream error_message;
    error_message << "curl error " << response.http_status_code << ": " << response.error_message;
    LOGGER_LOG(LVL_error, error_message.str());
    if (retry_times) {
      sleep(1);
      response = perform(curl_handler, --retry_times);
    }
  }
  LOGGER_LOG(LVL_trace, "response: " << response.body);
  return response;
}

HttpResponse HttpClient::download(const std::string& url, curl_write_callback callback, void* userp) {
  CURL* curl_download = curl_easy_duphandle(curl);
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
