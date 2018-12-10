#ifndef SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_

#include <list>
#include <map>

#include <boost/filesystem.hpp>

#include "ostree_hash.h"
#include "ostree_object.h"
#include "ostree_ref.h"
#include "ostree_repo.h"
#include "treehub_server.h"

class OSTreeHttpRepo : public OSTreeRepo {
 public:
  explicit OSTreeHttpRepo(const TreehubServer* server) : server_(server) {}

  bool LooksValid() const override;
  OSTreeObject::ptr GetObject(OSTreeHash hash, OstreeObjectType type) const override;
  OSTreeObject::ptr GetObject(const uint8_t sha256[32], OstreeObjectType type) const override;
  OSTreeRef GetRef(const std::string& refname) const override;

  const boost::filesystem::path root() const override { return root_.Path(); }

 private:
  bool Get(const boost::filesystem::path& path) const;
  bool CheckForObject(const OSTreeHash& hash, const std::string& path, OSTreeObject::ptr& object) const;
  static size_t curl_handle_write(void* buffer, size_t size, size_t nmemb, void* userp);

  typedef std::map<OSTreeHash, OSTreeObject::ptr> otable;
  mutable otable ObjectTable;  // Makes sure that the same commit object is not added twice
  const TreehubServer* server_;
  const TemporaryDirectory root_;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_
