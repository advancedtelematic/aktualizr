#include "httpclient.h"

#include <assert.h>

#include "logger.h"

/*****************************************************************************/
/**
 * \par Description:
 *    A writeback handler for the curl library. I handles writing response
 *    data from curl into a string.
 *    https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 */
static size_t writeString(void* contents, size_t size, size_t nmemb,
                          void* userp) {
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
static size_t writeDiscard(void*, size_t size, size_t nmemb) {
  return size * nmemb;
}

HttpClient::HttpClient() {
  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  headers = NULL;

  // let curl use our write function
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

  if (loggerGetSeverity() <= LVL_debug) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }
}

HttpClient::~HttpClient() {
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

bool HttpClient::authenticate(const AuthConfig& conf) {
  CURL* curl_auth = curl_easy_duphandle(curl);
  std::string auth_url = conf.server + "/token";
  curl_easy_setopt(curl_auth, CURLOPT_URL, auth_url.c_str());
  // let curl put the username and password using HTTP basic authentication
  curl_easy_setopt(curl_auth, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
  curl_easy_setopt(curl_auth, CURLOPT_POSTFIELDS,
                   "grant_type=client_credentials");

  // compose username and password
  std::string auth_header = conf.client_id + ":" + conf.client_secret;
  // forward username and password to curl
  curl_easy_setopt(curl_auth, CURLOPT_USERPWD, auth_header.c_str());

  LOGGER_LOG(LVL_debug,
             "servercon - requesting token from server: " << conf.server);

  std::string response;
  curl_easy_setopt(curl_auth, CURLOPT_WRITEDATA, (void*)&response);
  CURLcode result = curl_easy_perform(curl_auth);

  curl_easy_cleanup(curl_auth);
  if (result != CURLE_OK) {
    LOGGER_LOG(LVL_error, "authentication curl error: "
                              << result << " with server: " << conf.server
                              << "and auth header: " << auth_header);
    return false;
  }
  Json::Reader reader;
  Json::Value json;
  reader.parse(response, json);

  token = json["access_token"].asString();
  token_type = json["token_type"].asString();

  std::string header = "Authorization: Bearer " + token;
  headers = curl_slist_append(headers, header.c_str());
  headers = curl_slist_append(headers, "Accept: */*");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "charsets: utf-8");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  return true;
}

Json::Value HttpClient::get(const std::string& url) {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  return perform(curl);
}

Json::Value HttpClient::post(const std::string& url, const Json::Value& data) {
  return post(url, Json::FastWriter().write(data));
}

Json::Value HttpClient::post(const std::string& url, const std::string& data) {
  CURL* curl_post = curl_easy_duphandle(curl);
  curl_easy_setopt(curl_post, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_post, CURLOPT_POSTFIELDS, data.c_str());
  Json::Value result = perform(curl_post);
  curl_easy_cleanup(curl_post);
  return result;
}

Json::Value HttpClient::perform(CURL* curl_handler) {
  Json::Value json;
  std::string response;
  curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void*)&response);
  CURLcode result = curl_easy_perform(curl_handler);
  if (result != CURLE_OK) {
    LOGGER_LOG(LVL_error, "curl error: " << result);
    return json;
  }
  long http_code = 0;
  curl_easy_getinfo(curl_handler, CURLINFO_RESPONSE_CODE, &http_code);

  Json::Reader reader;
  reader.parse(response, json);
  return json;
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
  FILE* fp = fopen(filename.c_str(), "w");
  curl_easy_setopt(curl_download, CURLOPT_WRITEDATA, fp);
  result = curl_easy_perform(curl_download);
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
