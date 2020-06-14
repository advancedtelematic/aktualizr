#ifndef SOTA_CLIENT_TOOLS_TREEHUB_SERVER_H_
#define SOTA_CLIENT_TOOLS_TREEHUB_SERVER_H_

#include <string>

#include <curl/curl.h>
#include <libaktualizr/utils.h>

#include "server_credentials.h"

class TreehubServer {
 public:
  TreehubServer();
  void SetToken(const std::string &token);
  void SetContentType(const std::string &content_type);
  void SetCerts(const std::string &client_p12);
  void SetAuthBasic(const std::string &username, const std::string &password);

  void InjectIntoCurl(const std::string &url_suffix, CURL *curl_handle, bool tufrepo = false) const;

  void ca_certs(const std::string &cacerts) { ca_certs_ = cacerts; }
  void root_url(const std::string &_root_url);
  void repo_url(const std::string &_repo_url);
  std::string root_url() { return root_url_; };

 private:
  std::string ca_certs_;
  std::string root_url_;
  std::string repo_url_;
  std::string username_;
  std::string password_;
  std::string root_cert_;
  TemporaryFile client_p12_path_;
  AuthMethod method_{AuthMethod::kNone};
  struct curl_slist auth_header_ {};
  // Don't modify auth_header_contents_ without updating the pointer in
  // auth_header_
  std::string auth_header_contents_;
  struct curl_slist force_header_ {};
  // Don't modify force_header_contents_ without updating the pointer in
  // force_header_
  std::string force_header_contents_;
  struct curl_slist content_type_header_ {};
  // Don't modify content_type_header_contents_ without updating the pointer in
  // content_type_header_
  std::string content_type_header_contents_;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_TREEHUB_SERVER_H_
