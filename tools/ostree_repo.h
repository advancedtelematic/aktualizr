#ifndef SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
#define SOTA_CLIENT_TOOLS_OSTREE_REPO_H_

#include <boost/noncopyable.hpp>
#include <boost/array.hpp>
#include <list>
#include <map>

#include "ostree_object.h"

class OSTreeRepo : private boost::noncopyable {
 public:
  OSTreeRepo(std::string root_path) : root_(root_path) {}

  bool LooksValid() const;
  OSTreeObject::ptr GetObject(const uint8_t sha256[32]) const;

  const std::string root() const { return root_; }

 private:
  typedef std::map<boost::array<uint8_t, 32>, OSTreeObject::ptr> otable;
  mutable otable
      ObjectTable;  // Makes sure that the same commit object is not added twice
  const std::string root_;
  std::string hash_to_str(const uint8_t* sha256) const;
  boost::array<uint8_t, 32> hash_as_array(const uint8_t* sha256) const;
};

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_OSTREE_REPO_H_
