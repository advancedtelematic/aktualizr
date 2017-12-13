#ifndef SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_

#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <list>
#include <map>

#include "ostree_hash.h"
#include "ostree_object.h"
#include "ostree_ref.h"
#include "ostree_repo.h"
#include "treehub_server.h"

class OSTreeHttpRepo : public OSTreeRepo {
 public:
  explicit OSTreeHttpRepo(const TreehubServer* server) : server_(server) {}
  ~OSTreeHttpRepo() {}

  bool LooksValid() const;
  OSTreeObject::ptr GetObject(const OSTreeHash hash) const;
  OSTreeObject::ptr GetObject(const uint8_t sha256[32]) const;
  OSTreeRef GetRef(const std::string& refname) const;

  const std::string root() const { return root_.PathString(); }

 private:
  bool Get(const boost::filesystem::path& path) const;
  static size_t curl_handle_write(void* buffer, size_t size, size_t nmemb, void* userp);
  typedef std::map<OSTreeHash, OSTreeObject::ptr> otable;
  mutable otable ObjectTable;  // Makes sure that the same commit object is not added twice
  const TreehubServer* server_;
  const TemporaryDirectory root_;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_HTTP_REPO_H_
