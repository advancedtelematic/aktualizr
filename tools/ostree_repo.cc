#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "ostree_repo.h"

using std::string;
namespace fs = boost::filesystem;

bool OSTreeRepo::LooksValid() const {
  fs::path objects_dir(root_ + "/objects");
  fs::path refs_dir(root_ + "/refs");
  return fs::exists(objects_dir) && fs::exists(refs_dir);
}

boost::array<uint8_t, 32> OSTreeRepo::hash_as_array(
    const uint8_t* sha256) const {
  boost::array<uint8_t, 32> res;
  for (int i = 0; i < 32; i++) res[i] = sha256[i];
  return res;
}

std::string OSTreeRepo::hash_to_str(const uint8_t* sha256) const {
  std::stringstream str_str;
  str_str.fill('0');

  // sha256 hash is always 256 bits = 32 bytes long
  for (int i = 0; i < 32; i++)
    str_str << std::setw(2) << std::hex << (unsigned short)sha256[i];
  return str_str.str();
}

OSTreeObject::ptr OSTreeRepo::GetObject(const uint8_t sha256[32]) const {
  boost::array<uint8_t, 32> sha256_array = hash_as_array(sha256);
  otable::const_iterator it;
  it = ObjectTable.find(sha256_array);
  if (it != ObjectTable.end()) return it->second;

  std::string exts[] = {".filez", ".dirtree", ".dirmeta", ".commit"};
  std::string objpath = hash_to_str(sha256).insert(2, 1, '/');

  BOOST_FOREACH (std::string ext, exts) {
    if (fs::is_regular_file(root_ + "/objects/" + objpath + ext)) {
      OSTreeObject::ptr obj(new OSTreeObject(*this, objpath + ext));
      ObjectTable[sha256_array] = obj;
      return obj;
    }
  }

  return NULL;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
