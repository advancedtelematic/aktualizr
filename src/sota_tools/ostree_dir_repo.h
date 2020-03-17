#ifndef SOTA_CLIENT_TOOLS_OSTREE_DIR_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_DIR_REPO_H_

#include <boost/filesystem.hpp>

#include "ostree_ref.h"
#include "ostree_repo.h"

class OSTreeDirRepo : public OSTreeRepo {
 public:
  explicit OSTreeDirRepo(boost::filesystem::path root_path) : root_(std::move(root_path)) {}
  ~OSTreeDirRepo() override = default;

  bool LooksValid() const override;
  OSTreeRef GetRef(const std::string& refname) const override;
  boost::filesystem::path root() const override { return root_; }

 private:
  bool FetchObject(const boost::filesystem::path& path) const override;

  const boost::filesystem::path root_;
};

#endif  // SOTA_CLIENT_TOOLS_OSTREE_DIR_REPO_H_

// vim: set tabstop=2 shiftwidth=2 expandtab:
