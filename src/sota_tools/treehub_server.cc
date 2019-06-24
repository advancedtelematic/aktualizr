#include "treehub_server.h"

#include <assert.h>

#include <iostream>

#include <boost/algorithm/string.hpp>

using std::string;

TreehubServer::TreehubServer() {
  auth_header_.data = const_cast<char*>(auth_header_contents_.c_str());
  auth_header_.next = &force_header_;
  force_header_contents_ = "x-ats-ostree-force: true";
  force_header_.data = const_cast<char*>(force_header_contents_.c_str());
  force_header_.next = nullptr;
}

void TreehubServer::SetToken(const string& token) {
  assert(auth_header_.next == &force_header_);
  assert(force_header_.next == nullptr);

  auth_header_contents_ = "Authorization: Bearer " + token;
  auth_header_.data = const_cast<char*>(auth_header_contents_.c_str());
  method_ = AuthMethod::kOauth2;
}

void TreehubServer::SetCerts(const std::string& root_cert, const std::string& client_cert,
                             const std::string& client_key) {
  method_ = AuthMethod::kTls;
  root_cert_ = root_cert;
  client_cert_path_.PutContents(client_cert);
  client_key_path_.PutContents(client_key);
}

void TreehubServer::SetAuthBasic(const std::string& username, const std::string& password) {
  method_ = AuthMethod::kBasic;
  username_ = username;
  password_ = password;
}

// Note that this method creates a reference from curl_handle to this.  Keep
// this TreehubServer object alive until the curl request has been completed
void TreehubServer::InjectIntoCurl(const string& url_suffix, CURL* curl_handle, const bool tufrepo) const {
  std::string url = (tufrepo ? repo_url_ : root_url_);

  if (*url.rbegin() != '/' && *url_suffix.begin() != '/') {
    url += "/";
  } else if (*url.rbegin() == '/' && *url_suffix.begin() == '/') {
    url.erase(url.length() - 1);
  }

  boost::trim_if(url, boost::is_any_of(" \t\r\n"));

  curlEasySetoptWrapper(curl_handle, CURLOPT_URL, (url + url_suffix).c_str());

  curlEasySetoptWrapper(curl_handle, CURLOPT_HTTPHEADER, &auth_header_);
  // If we need authentication but don't have an OAuth2 token or TLS
  // credentials, fall back to legacy username/password.
  if (method_ == AuthMethod::kBasic) {
    curlEasySetoptWrapper(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curlEasySetoptWrapper(curl_handle, CURLOPT_USERNAME, username_.c_str());
    curlEasySetoptWrapper(curl_handle, CURLOPT_PASSWORD, password_.c_str());
  }

  std::string all_root_certs;
  if (method_ == AuthMethod::kTls) {
    curlEasySetoptWrapper(curl_handle, CURLOPT_SSL_VERIFYPEER, 1);
    curlEasySetoptWrapper(curl_handle, CURLOPT_SSL_VERIFYHOST, 2);
    curlEasySetoptWrapper(curl_handle, CURLOPT_USE_SSL, CURLUSESSL_ALL);

    curlEasySetoptWrapper(curl_handle, CURLOPT_SSLCERT, client_cert_path_.PathString().c_str());
    curlEasySetoptWrapper(curl_handle, CURLOPT_SSLKEY, client_key_path_.PathString().c_str());
    if (!root_cert_.empty()) {
      all_root_certs = root_cert_;
    }
  }
  if (!ca_certs_.empty()) {
    all_root_certs += Utils::readFile(ca_certs_);
  }
  if (!all_root_certs.empty()) {
    Utils::writeFile(root_cert_path_.PathString(), all_root_certs);
    curlEasySetoptWrapper(curl_handle, CURLOPT_CAINFO, root_cert_path_.PathString().c_str());
    curlEasySetoptWrapper(curl_handle, CURLOPT_CAPATH, NULL);
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
