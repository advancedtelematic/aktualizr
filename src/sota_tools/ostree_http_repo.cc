#include <fcntl.h>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "logging/logging.h"
#include "ostree_hash.h"
#include "ostree_http_repo.h"

using std::string;
namespace pt = boost::property_tree;

bool OSTreeHttpRepo::LooksValid() const {
  if (Get("config")) {
    pt::ptree config;
    try {
      pt::read_ini((root_ / "config").string(), config);
      if (config.get<string>("core.mode") != "archive-z2") {
        LOG_WARNING << "OSTree repo is not in archive-z2 format";
        return false;
      }
      return true;

    } catch (const pt::ini_parser_error &error) {
      LOG_WARNING << "Couldn't parse OSTree config file: " << (root_ / "config").string();
      return false;
    } catch (const pt::ptree_error &error) {
      LOG_WARNING << "Could not find core.mode in OSTree config file";
      return false;
    }
  } else {
    return false;
  }
}

OSTreeRef OSTreeHttpRepo::GetRef(const std::string &refname) const { return OSTreeRef(*server_, refname); }

OSTreeObject::ptr OSTreeHttpRepo::GetObject(const uint8_t sha256[32]) const { return GetObject(OSTreeHash(sha256)); }

bool OSTreeHttpRepo::Get(const boost::filesystem::path &path) const {
  CURL *easy_handle = curl_easy_init();
  curl_easy_setopt(easy_handle, CURLOPT_VERBOSE, get_curlopt_verbose());
  server_->InjectIntoCurl(path.string(), easy_handle);
  curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, &OSTreeHttpRepo::curl_handle_write);
  boost::filesystem::create_directories((root_ / path).parent_path());
  int fp = open((root_ / path).c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
  curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, &fp);
  curl_easy_setopt(easy_handle, CURLOPT_FAILONERROR, true);
  CURLcode err = curl_easy_perform(easy_handle);
  close(fp);
  curl_easy_cleanup(easy_handle);
  if (err != 0u) {
    remove((root_ / path).c_str());
    return false;
  }
  return true;
}

OSTreeObject::ptr OSTreeHttpRepo::GetObject(const OSTreeHash hash) const {
  otable::const_iterator it;
  it = ObjectTable.find(hash);
  if (it != ObjectTable.end()) {
    return it->second;
  }

  std::string exts[] = {".filez", ".dirtree", ".dirmeta", ".commit"};
  std::string objpath = hash.string().insert(2, 1, '/');

  for (const std::string &ext : exts) {
    if (Get(std::string("objects/") += objpath += ext)) {
      OSTreeObject::ptr obj(new OSTreeObject(*this, objpath + ext));
      ObjectTable[hash] = obj;
      return obj;
    }
  }
  throw OSTreeObjectMissing(hash);
}

size_t OSTreeHttpRepo::curl_handle_write(void *buffer, size_t size, size_t nmemb, void *userp) {
  return write(*static_cast<int *>(userp), buffer, nmemb * size);
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
