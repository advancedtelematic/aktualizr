#include "ostree_dir_repo.h"

#include <string>

#include <boost/property_tree/ini_parser.hpp>

#include "logging/logging.h"

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
      if (config.get<std::string>("core.mode") != "archive-z2") {
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

bool OSTreeDirRepo::FetchObject(const boost::filesystem::path &path) const {
  return fs::is_regular_file((root_ / path).string());
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
