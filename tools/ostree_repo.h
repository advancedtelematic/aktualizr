#ifndef SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_REPO_H_

#include <boost/noncopyable.hpp>
#include <list>

#include "ostree_object.h"
#include "ostree_ref.h"

class OSTreeRepo : private boost::noncopyable {
 public:
  OSTreeRepo(std::string root) : root_(root) {}

  bool LooksValid() const;
  void FindAllObjects(std::list<OSTreeObject::ptr>* objects) const;

  OSTreeRef Ref(const std::string& refname) const;

 private:
  std::string root_;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
