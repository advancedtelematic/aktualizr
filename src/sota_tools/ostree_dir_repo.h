#ifndef SOTA_CLIENT_TOOLS_OSTREE_DIR_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_DIR_REPO_H_

#include <list>
#include <map>
#include <utility>

#include <boost/filesystem.hpp>

#include "ostree_hash.h"
#include "ostree_object.h"
#include "ostree_ref.h"
#include "ostree_repo.h"

class OSTreeDirRepo : public OSTreeRepo {
 public:
  explicit OSTreeDirRepo(boost::filesystem::path root_path) : root_(std::move(root_path)) {}

  bool LooksValid() const override;
  OSTreeObject::ptr GetObject(OSTreeHash hash, OstreeObjectType type) const override;
  OSTreeObject::ptr GetObject(const uint8_t sha256[32], OstreeObjectType type) const override;
  OSTreeRef GetRef(const std::string &refname) const override;

  const boost::filesystem::path root() const override { return root_; }

 private:
  typedef std::map<OSTreeHash, OSTreeObject::ptr> otable;
  mutable otable ObjectTable;  // Makes sure that the same commit object is not added twice
  const boost::filesystem::path root_;
};

#endif  // SOTA_CLIENT_TOOLS_OSTREE_DIR_REPO_H_

// vim: set tabstop=2 shiftwidth=2 expandtab:
