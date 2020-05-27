#ifndef SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_REPO_H_

#include <map>
#include <string>

#include <boost/filesystem.hpp>

#include "garage_common.h"
#include "ostree_hash.h"
#include "ostree_object.h"

class OSTreeRef;

/**
 * A source repository to read OSTree objects from. This can be either a directory
 * on disk, or a URL in the garage-deploy case.
 */
class OSTreeRepo {
 public:
  using ptr = std::shared_ptr<OSTreeRepo>;
  OSTreeRepo& operator=(const OSTreeRepo&) = delete;

  virtual ~OSTreeRepo() = default;
  virtual bool LooksValid() const = 0;
  virtual boost::filesystem::path root() const = 0;
  virtual OSTreeRef GetRef(const std::string& refname) const = 0;

  OSTreeObject::ptr GetObject(OSTreeHash hash, OstreeObjectType type) const;
  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  OSTreeObject::ptr GetObject(const uint8_t sha256[32], OstreeObjectType type) const;

 protected:
  virtual bool FetchObject(const boost::filesystem::path& path) const = 0;

  bool CheckForObject(const OSTreeHash& hash, const std::string& path, OSTreeObject::ptr& object) const;

  typedef std::map<OSTreeHash, OSTreeObject::ptr> otable;
  mutable otable ObjectTable;  // Makes sure that the same commit object is not added twice
};

/**
 * Thrown by GetObject when the object requested is not present in the
 * repository.
 */
class OSTreeObjectMissing : std::exception {
 public:
  explicit OSTreeObjectMissing(const OSTreeHash _missing_object) : missing_object_(_missing_object) {}

  const char* what() const noexcept override { return "OSTree repository is missing an object"; }

  OSTreeHash missing_object() const { return missing_object_; }

 private:
  OSTreeHash missing_object_;
};
// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
