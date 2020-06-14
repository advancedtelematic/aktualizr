#ifndef SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_

#include <boost/filesystem.hpp>
#include <libaktualizr/utils.h>

#include "logging/logging.h"
#include "ostree_ref.h"
#include "ostree_repo.h"
#include "treehub_server.h"

class OSTreeHttpRepo : public OSTreeRepo {
 public:
  explicit OSTreeHttpRepo(TreehubServer* server, const boost::filesystem::path& root_in = "")
      : server_(server), root_(root_in) {
    if (root_.empty()) {
      root_ = root_tmp_.Path();
    }
    curlEasySetoptWrapper(easy_handle_.get(), CURLOPT_VERBOSE, get_curlopt_verbose());
    curlEasySetoptWrapper(easy_handle_.get(), CURLOPT_WRITEFUNCTION, &OSTreeHttpRepo::curl_handle_write);
    curlEasySetoptWrapper(easy_handle_.get(), CURLOPT_FAILONERROR, true);
  }
  ~OSTreeHttpRepo() override = default;

  bool LooksValid() const override;
  OSTreeRef GetRef(const std::string& refname) const override;
  boost::filesystem::path root() const override { return root_; }

 private:
  bool FetchObject(const boost::filesystem::path& path) const override;
  static size_t curl_handle_write(void* buffer, size_t size, size_t nmemb, void* userp);

  TreehubServer* server_;
  boost::filesystem::path root_;
  const TemporaryDirectory root_tmp_;
  mutable CurlEasyWrapper easy_handle_;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_
