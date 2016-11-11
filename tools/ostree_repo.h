#ifndef SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_REPO_H_

#include <boost/noncopyable.hpp>
#include <list>

#include "ostree_object.h"

class OSTreeRepo : private boost::noncopyable {
 public:
  OSTreeRepo(std::string root_path) : root_(root_path) {}

  bool LooksValid() const;
  void FindAllObjects(std::list<OSTreeObject::ptr>* objects) const;
  OSTreeObject::ptr GetObject(const uint8_t* sha256) const;

  const std::string root() const { return root_; }

 private:
  const std::string root_;
  std::string hash_to_str(const uint8_t* sha256) const;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
