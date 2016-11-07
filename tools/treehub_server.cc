#include <assert.h>

#include "treehub_server.h"

using std::string;

TreehubServer::TreehubServer() : using_auth_plus(false) {
  auth_header_.data = NULL;
  auth_header_.next = NULL;
}

void TreehubServer::SetToken(const string& token) {
  assert(auth_header_.next == NULL);

  auth_header_contents_ = "Authorization: Bearer " + token;
  auth_header_.data = const_cast<char*>(auth_header_contents_.c_str());
  using_auth_plus = true;
}

// Note that this method creates a reference from curl_handle to this.  Keep
// this TreehubServer object alive until the curl request has been completed
void TreehubServer::InjectIntoCurl(const string& url_suffix,
                                   CURL* curl_handle) const {
  curl_easy_setopt(curl_handle, CURLOPT_URL, (root_url + url_suffix).c_str());

  if (using_auth_plus) {  // If we have a Auth+ token, use that, otherwise
                          // fallback to legacy username/password
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, &auth_header_);
  } else {
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl_handle, CURLOPT_USERNAME, username.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, password.c_str());
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
