#include <assert.h>

#include "treehub_server.h"

using std::string;

TreehubServer::TreehubServer() : using_oauth2_(false) {
  auth_header_.data = const_cast<char*>(auth_header_contents_.c_str());
  auth_header_.next = &force_header_;
  force_header_contents_ = "x-ats-ostree-force: true";
  force_header_.data = const_cast<char*>(force_header_contents_.c_str());
  force_header_.next = NULL;
}

void TreehubServer::SetToken(const string& token) {
  assert(auth_header_.next == &force_header_);
  assert(force_header_.next == NULL);

  auth_header_contents_ = "Authorization: Bearer " + token;
  auth_header_.data = const_cast<char*>(auth_header_contents_.c_str());
  using_oauth2_ = true;
}

// Note that this method creates a reference from curl_handle to this.  Keep
// this TreehubServer object alive until the curl request has been completed
void TreehubServer::InjectIntoCurl(const string& url_suffix,
                                   CURL* curl_handle) const {
  curl_easy_setopt(curl_handle, CURLOPT_URL, (root_url_ + url_suffix).c_str());

  curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, &auth_header_);
  if (!using_oauth2_) {  // If we don't have a OAuth2 token fallback to legacy
                         // username/password
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl_handle, CURLOPT_USERNAME, username_.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, password_.c_str());
  }

  if (ca_certs_ != "") {
    curl_easy_setopt(curl_handle, CURLOPT_CAINFO, ca_certs_.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_CAPATH, NULL);
  }
}

// Set the url of the treehub server, this should be something like
// "https://treehub-staging.atsgarage.com/api/v2/"
// The trailing slash is optional, and will be appended if required
void TreehubServer::root_url(const std::string& root_url) {
  root_url_ = root_url;
  if (root_url_.size() > 0 && root_url_[root_url_.size() - 1] != '/') {
    root_url_.append("/");
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
