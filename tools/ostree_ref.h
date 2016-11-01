#ifndef SOTA_CLIENT_TOOLS_OSTREE_REF_H_
#define SOTA_CLIENT_TOOLS_OSTREE_REF_H_

#include <boost/filesystem.hpp>
#include <curl/curl.h>
#include <sstream>

#include "treehub_server.h"

class OSTreeRef {
 public:
  OSTreeRef(const std::string repo_root, const std::string ref_name);

  void PushRef(const TreehubServer& push_target, CURL* curl_easy_handle);

  bool IsValid() const;

 private:
  std::string Url() const;
  std::string RefContent() const;

  const boost::filesystem::path file_path_;  // Full path to the object
  const std::string ref_name_;               // OSTree name of the object
  std::stringstream http_response_;

  static size_t curl_handle_write(void* buffer, size_t size, size_t nmemb,
                                  void* userp);
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_REF_H_
