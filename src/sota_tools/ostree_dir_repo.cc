#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "logging/logging.h"
#include "ostree_dir_repo.h"
#include "ostree_hash.h"

using std::string;
namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

bool OSTreeDirRepo::LooksValid() const {
  fs::path objects_dir(root_ / "/objects");
  fs::path refs_dir(root_ / "/refs");
  fs::path config_file(root_ / "/config");
  if (fs::is_directory(objects_dir) && fs::is_directory(refs_dir) && fs::is_regular(config_file)) {
    pt::ptree config;
    try {
      pt::read_ini(config_file.string(), config);
      if (config.get<string>("core.mode") != "archive-z2") {
        LOG_WARNING << "OSTree repo is not in archive-z2 format";
        return false;
      }
      return true;

    } catch (const pt::ini_parser_error &error) {
      LOG_WARNING << "Couldn't parse OSTree config file: " << config_file;
      return false;
    } catch (const pt::ptree_error &error) {
      LOG_WARNING << "Could not find core.mode in OSTree config file";
      return false;
    }
  } else {
    return false;
  }
}

OSTreeRef OSTreeDirRepo::GetRef(const std::string &refname) const { return OSTreeRef(*this, refname); }

OSTreeObject::ptr OSTreeDirRepo::GetObject(const uint8_t sha256[32]) const { return GetObject(OSTreeHash(sha256)); }

OSTreeObject::ptr OSTreeDirRepo::GetObject(const OSTreeHash hash) const {
  otable::const_iterator it;
  it = ObjectTable.find(hash);
  if (it != ObjectTable.end()) {
    return it->second;
  }

  std::string exts[] = {".filez", ".dirtree", ".dirmeta", ".commit"};
  boost::filesystem::path objpath = hash.string().insert(2, 1, '/');

  for (const std::string &ext : exts) {
    if (fs::is_regular_file((root_ / "/objects/" / objpath).string() + ext)) {
      OSTreeObject::ptr obj(new OSTreeObject(*this, objpath.string() + ext));
      ObjectTable[hash] = obj;
      return obj;
    }
  }
  throw OSTreeObjectMissing(hash);
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
