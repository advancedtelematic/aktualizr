#ifndef SOTA_CLIENT_TOOLS_TREEHUB_SERVER_H_
#define SOTA_CLIENT_TOOLS_TREEHUB_SERVER_H_

#include <curl/curl.h>
#include <string>

class TreehubServer {
 public:
  void InjectIntoCurl(const std::string &url_suffix, CURL *curl_handle) const;
  std::string root_url;
  std::string username;
  std::string password;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_TREEHUB_SERVER_H_
