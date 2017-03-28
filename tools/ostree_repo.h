#ifndef SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_REPO_H_

#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <list>
#include <map>

#include "ostree_object.h"
#include "ostree_hash.h"

class OSTreeRepo : private boost::noncopyable {
 public:
  OSTreeRepo(std::string root_path) : root_(root_path) {}

  bool LooksValid() const;
  OSTreeObject::ptr GetObject(const OSTreeHash hash) const;
  OSTreeObject::ptr GetObject(const uint8_t sha256[32]) const;

  const std::string root() const { return root_; }

 private:
  typedef std::map<OSTreeHash, OSTreeObject::ptr> otable;
  mutable otable
      ObjectTable;  // Makes sure that the same commit object is not added twice
  const std::string root_;
};

/**
 * Thrown by GetObject when the object requested is not present in the
 * repository.
 */
class OSTreeObjectMissing : std::exception {
 public:
  OSTreeObjectMissing(const OSTreeHash missing_object)
      : missing_object_(missing_object) {}

  virtual const char* what() const noexcept {
    return "OSTree repository is missing an object";
  }

  OSTreeHash missing_object() const { return missing_object_; }

 private:
  OSTreeHash missing_object_;
};
// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
