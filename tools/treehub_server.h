#ifndef SOTA_CLIENT_TOOLS_TREEHUB_SERVER_H_
#define SOTA_CLIENT_TOOLS_TREEHUB_SERVER_H_

#include <curl/curl.h>
#include <string>

class TreehubServer {
 public:
  TreehubServer();
  void SetToken(const std::string &authentication_token);
  void InjectIntoCurl(const std::string &url_suffix, CURL *curl_handle) const;

  void ca_certs(const std::string &cacerts) { ca_certs_ = cacerts; }
  void root_url(const std::string &root_url);
  void username(const std::string &username) { username_ = username; }
  void password(const std::string &password) { password_ = password; }

 private:
  std::string ca_certs_;
  std::string root_url_;
  std::string username_;
  std::string password_;
  bool using_oauth2_;
  struct curl_slist auth_header_;
  // Don't modify auth_header_contents_ without updating the pointer in
  // auth_header_
  std::string auth_header_contents_;
  struct curl_slist force_header_;
  // Don't modify force_header_contents_ without updating the pointer in
  // force_header_
  std::string force_header_contents_;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_TREEHUB_SERVER_H_
