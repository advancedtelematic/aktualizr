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

HttpClient::HttpClient() : authenticated(false), user_agent(std::string("Aktualizr/") + AKTUALIZR_VERSION) {
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

HttpClient::HttpClient(const HttpClient& curl_in) : authenticated(false) {
  curl = curl_easy_duphandle(curl_in.curl);
  token = curl_in.token;

  struct curl_slist* inlist = curl_in.headers;
  headers = NULL;
  struct curl_slist* tmp;

  while (inlist) {
    tmp = curl_slist_append(headers, inlist->data);

    if (!tmp) {
      curl_slist_free_all(headers);
      return;
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
  authenticated = true;
  return true;
}

bool HttpClient::authenticate(const AuthConfig& conf) {
  CURL* curl_auth = curl_easy_duphandle(curl);
  std::string auth_url = conf.server + "/token";
  curl_easy_setopt(curl_auth, CURLOPT_URL, auth_url.c_str());
  // let curl put the username and password using HTTP basic authentication
  curl_easy_setopt(curl_auth, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
  curl_easy_setopt(curl_auth, CURLOPT_POSTFIELDS, "grant_type=client_credentials");

  // compose username and password
  std::string auth_header = conf.client_id + ":" + conf.client_secret;
  // forward username and password to curl
  curl_easy_setopt(curl_auth, CURLOPT_USERPWD, auth_header.c_str());

  LOGGER_LOG(LVL_debug, "servercon - requesting token from server: " << conf.server);

  std::string response;
  curl_easy_setopt(curl_auth, CURLOPT_WRITEDATA, (void*)&response);

  curl_slist* h = curl_slist_append(NULL, "Content-Type: application/x-www-form-urlencoded");
  h = curl_slist_append(h, "charsets: utf-8");
  curl_easy_setopt(curl_auth, CURLOPT_HTTPHEADER, h);

  CURLcode result = curl_easy_perform(curl_auth);
  LOGGER_LOG(LVL_trace, "response:" << response);

  curl_easy_cleanup(curl_auth);
  if (result != CURLE_OK) {
    LOGGER_LOG(LVL_error, "authentication curl error: " << result << " with server: " << conf.server
                                                        << "and auth header: " << auth_header);
    return false;
  }
  Json::Reader reader;
  Json::Value json;
  reader.parse(response, json);

  token = json["access_token"].asString();

  std::string header = "Authorization: Bearer " + token;
  headers = curl_slist_append(headers, header.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  authenticated = true;
  return true;
}

std::string HttpClient::get(const std::string& url) {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  LOGGER_LOG(LVL_debug, "GET " << url);
  return perform(curl, RETRY_TIMES);
}

Json::Value HttpClient::getJson(const std::string& url) {
  Json::Value json;
  Json::Reader reader;

  std::string response = get(url);
  reader.parse(response, json);
  return json;
}

Json::Value HttpClient::post(const std::string& url, const Json::Value& data) {
  Json::Value json;
  Json::Reader reader;

  std::string response = post(url, Json::FastWriter().write(data));
  reader.parse(response, json);
  return json;
}

void HttpClient::setCerts(const std::string& ca, const std::string& cert, const std::string& pkey) {
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, true);
  curl_easy_setopt(curl, CURLOPT_CAINFO, ca.c_str());
  curl_easy_setopt(curl, CURLOPT_SSLCERT, cert.c_str());
  curl_easy_setopt(curl, CURLOPT_SSLKEY, pkey.c_str());
}

std::string HttpClient::post(const std::string& url, const std::string& data) {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
  LOGGER_LOG(LVL_trace, "post request body:" << data);
  std::string result = perform(curl, RETRY_TIMES);
  return result;
}

std::string HttpClient::put(const std::string& url, const std::string& data) {
  CURL* curl_put = curl_easy_duphandle(curl);

  curl_easy_setopt(curl_put, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_put, CURLOPT_POSTFIELDS, data.c_str());
  curl_easy_setopt(curl_put, CURLOPT_CUSTOMREQUEST, "PUT");
  LOGGER_LOG(LVL_trace, "put request body:" << data);
  std::string result = perform(curl_put, RETRY_TIMES);
  curl_easy_cleanup(curl_put);
  return result;
}

std::string HttpClient::perform(CURL* curl_handler, int retry_times) {
  std::string response;
  curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void*)&response);
  CURLcode result = curl_easy_perform(curl_handler);
  if (result != CURLE_OK) {
    std::ostringstream error_message;
    error_message << "curl error: " << curl_easy_strerror(result);
    LOGGER_LOG(LVL_error, error_message.str());
    if (retry_times) {
      sleep(1);
      perform(curl_handler, --retry_times);
    } else {
      throw std::runtime_error(error_message.str());
    }
  }
  curl_easy_getinfo(curl_handler, CURLINFO_RESPONSE_CODE, &http_code);
  LOGGER_LOG(LVL_trace, "response: " << response);
  return response;
}

bool HttpClient::download(const std::string& url, const std::string& filename) {
  // Download an update from SOTA Server. This requires two requests: the first
  // is to the sota server, and needs an Authentication: header to be present.
  // This request will return a 30x redirect to S3, which we must not send our
  // secret to (s3 also throws an error in this case :)

  CURL* curl_redir = curl_easy_duphandle(curl);
  curl_easy_setopt(curl_redir, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_redir, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl_redir, CURLOPT_WRITEFUNCTION, writeDiscard);

  CURLcode result = curl_easy_perform(curl_redir);
  if (result != CURLE_OK) {
    LOGGER_LOG(LVL_error, "curl download error: " << result);
    curl_easy_cleanup(curl_redir);
    return false;
  }

  char* newurl;
  curl_easy_getinfo(curl_redir, CURLINFO_REDIRECT_URL, &newurl);
  LOGGER_LOG(LVL_debug, "AWS Redirect URL:" << newurl);

  // Now perform the actual download
  CURL* curl_download = curl_easy_init();
  curl_easy_setopt(curl_download, CURLOPT_URL, newurl);
  // Let AWS Redirect us
  curl_easy_setopt(curl_download, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl_download, CURLOPT_WRITEFUNCTION, writeFile);

  struct stat st;
  std::string mode = "w";
  if (!stat(filename.c_str(), &st)) {
    curl_easy_setopt(curl_download, CURLOPT_RESUME_FROM, st.st_size);
    mode = "a";
  }

  FILE* fp = fopen(filename.c_str(), mode.c_str());
  curl_easy_setopt(curl_download, CURLOPT_WRITEDATA, fp);
  result = curl_easy_perform(curl_download);

  if (result == CURLE_RANGE_ERROR) {
    fseek(fp, 0, SEEK_SET);
    curl_easy_setopt(curl_download, CURLOPT_RESUME_FROM, 0);
    result = curl_easy_perform(curl_download);
  }
  curl_easy_cleanup(curl_download);
  curl_easy_cleanup(curl_redir);  // Keep newurl alive
  fclose(fp);

  if (result != CURLE_OK) {
    LOGGER_LOG(LVL_error, "curl download error: " << result);
    return false;
  }
  return true;
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
