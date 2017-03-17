#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "logging.h"
#include "ostree_hash.h"
#include "ostree_repo.h"

using std::string;
namespace fs = boost::filesystem;

bool OSTreeRepo::LooksValid() const {
  fs::path objects_dir(root_ + "/objects");
  fs::path refs_dir(root_ + "/refs");
  return fs::exists(objects_dir) && fs::exists(refs_dir);
}

OSTreeObject::ptr OSTreeRepo::GetObject(const uint8_t sha256[32]) const {
  return GetObject(OSTreeHash(sha256));
}

OSTreeObject::ptr OSTreeRepo::GetObject(const OSTreeHash hash) const {
  otable::const_iterator it;
  it = ObjectTable.find(hash);
  if (it != ObjectTable.end()) {
    return it->second;
  }

  std::string exts[] = {".filez", ".dirtree", ".dirmeta", ".commit"};
  std::string objpath = hash.string().insert(2, 1, '/');

  BOOST_FOREACH (std::string ext, exts) {
    if (fs::is_regular_file(root_ + "/objects/" + objpath + ext)) {
      OSTreeObject::ptr obj(new OSTreeObject(*this, objpath + ext));
      ObjectTable[hash] = obj;
      return obj;
    }
  }
  throw OSTreeObjectMissing(hash);
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
