#include <assert.h>
#include <iostream>

#include "treehub_server.h"
#include "utils.h"

using std::string;

TreehubServer::TreehubServer() : using_oauth2_(false), using_certs_(false) {
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

void TreehubServer::SetCerts(const std::string& root_cert, const std::string& client_cert,
                             const std::string& client_key) {
  using_certs_ = true;
  root_cert_ = root_cert;
  client_cert_path_.PutContents(client_cert);
  client_key_path_.PutContents(client_key);
}

// Note that this method creates a reference from curl_handle to this.  Keep
// this TreehubServer object alive until the curl request has been completed
void TreehubServer::InjectIntoCurl(const string& url_suffix, CURL* curl_handle, bool tufrepo) const {
  std::string url = (tufrepo ? repo_url_ : root_url_);

  if (*url.rbegin() != '/' && *url_suffix.begin() != '/')
    url += "/";
  else if (*url.rbegin() == '/' && *url_suffix.begin() == '/')
    url.erase(url.length() - 1);

  curl_easy_setopt(curl_handle, CURLOPT_URL, (url + url_suffix).c_str());

  curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, &auth_header_);
  if (!using_oauth2_) {  // If we don't have a OAuth2 token fallback to legacy
                         // username/password
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl_handle, CURLOPT_USERNAME, username_.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, password_.c_str());
  }

  std::string all_root_certs;
  if (using_certs_) {
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 1);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2);
    curl_easy_setopt(curl_handle, CURLOPT_USE_SSL, CURLUSESSL_ALL);

    curl_easy_setopt(curl_handle, CURLOPT_SSLCERT, client_cert_path_.PathString().c_str());
    curl_easy_setopt(curl_handle, CURLOPT_SSLKEY, client_key_path_.PathString().c_str());
    if (!root_cert_.empty()) {
      all_root_certs = root_cert_;
    }
  }
  if (!ca_certs_.empty()) {
    all_root_certs += Utils::readFile(ca_certs_);
  }
  if (!all_root_certs.empty()) {
    Utils::writeFile(root_cert_path_.PathString(), all_root_certs);
    curl_easy_setopt(curl_handle, CURLOPT_CAINFO, root_cert_path_.PathString().c_str());
    curl_easy_setopt(curl_handle, CURLOPT_CAPATH, NULL);
  }
}

// Set the url of the treehub server, this should be something like
// "https://treehub-staging.atsgarage.com/api/v2/"
// The trailing slash is optional, and will be appended if required
void TreehubServer::root_url(const std::string& _root_url) {
  root_url_ = _root_url;
  if (root_url_.size() > 0 && root_url_[root_url_.size() - 1] != '/') {
    root_url_.append("/");
  }
}

void TreehubServer::repo_url(const std::string& _repo_url) {
  repo_url_ = _repo_url;
  if (repo_url_.size() > 0 && repo_url_[repo_url_.size() - 1] != '/') {
    repo_url_.append("/");
  }
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
