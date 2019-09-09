#ifndef SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_

#include <boost/filesystem.hpp>

#include "ostree_ref.h"
#include "ostree_repo.h"
#include "treehub_server.h"

class OSTreeHttpRepo : public OSTreeRepo {
 public:
  explicit OSTreeHttpRepo(TreehubServer* server) : server_(server) {}
  ~OSTreeHttpRepo() override = default;

  bool LooksValid() const override;
  OSTreeRef GetRef(const std::string& refname) const override;
  const boost::filesystem::path root() const override { return root_.Path(); }

 private:
  bool FetchObject(const boost::filesystem::path& path) const override;
  static size_t curl_handle_write(void* buffer, size_t size, size_t nmemb, void* userp);

  TreehubServer* server_;
  const TemporaryDirectory root_;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_
