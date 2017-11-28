#ifndef SOTA_CLIENT_TOOLS_OSTREE_REF_H_
#define SOTA_CLIENT_TOOLS_OSTREE_REF_H_

#include <curl/curl.h>
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <sstream>

#include "ostree_hash.h"
#include "ostree_repo.h"
#include "treehub_server.h"

class OSTreeRef {
 public:
  OSTreeRef(const OSTreeRepo& root, const std::string& ref_name);
  OSTreeRef(const TreehubServer& serve_repo, const std::string& ref_name);
  OSTreeRef(OSTreeRef&& rhs) : ref_content_(rhs.ref_content_), ref_name_(rhs.ref_name_) {}

  void PushRef(const TreehubServer& push_target, CURL* curl_easy_handle);

  OSTreeHash GetHash() const;
  std::string GetName() const { return ref_name_; };

  bool IsValid() const;

 private:
  std::string Url() const;
  bool is_valid;

  std::string ref_content_;
  const std::string ref_name_;  // OSTree name of the object
  std::stringstream http_response_;

  static size_t curl_handle_write(void* buffer, size_t size, size_t nmemb, void* userp);
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_REF_H_
