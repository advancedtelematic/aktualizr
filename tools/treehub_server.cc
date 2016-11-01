#include "treehub_server.h"

using std::string;

void TreehubServer::InjectIntoCurl(const string& url_suffix,
                                   CURL* curl_handle) const {
  curl_easy_setopt(curl_handle, CURLOPT_URL, (root_url + url_suffix).c_str());
  curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl_handle, CURLOPT_USERNAME, username.c_str());
  curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, password.c_str());
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
