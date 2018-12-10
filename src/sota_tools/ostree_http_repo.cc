#include "ostree_http_repo.h"

#include <fcntl.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "logging/logging.h"
#include "utilities/utils.h"

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

OSTreeObject::ptr OSTreeHttpRepo::GetObject(const uint8_t sha256[32], const OstreeObjectType type) const {
  return GetObject(OSTreeHash(sha256), type);
}

bool OSTreeHttpRepo::Get(const boost::filesystem::path &path) const {
  CURLcode err = CURLE_OK;
  CurlEasyWrapper easy_handle;
  curlEasySetoptWrapper(easy_handle.get(), CURLOPT_VERBOSE, get_curlopt_verbose());
  server_->InjectIntoCurl(path.string(), easy_handle.get());
  curlEasySetoptWrapper(easy_handle.get(), CURLOPT_WRITEFUNCTION, &OSTreeHttpRepo::curl_handle_write);
  boost::filesystem::create_directories((root_ / path).parent_path());
  std::string filename = (root_ / path).string();
  int fp = open(filename.c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
  if (fp == -1) {
    LOG_ERROR << "Failed to open file: " << filename;
    return false;
  }
  curlEasySetoptWrapper(easy_handle.get(), CURLOPT_WRITEDATA, &fp);
  curlEasySetoptWrapper(easy_handle.get(), CURLOPT_FAILONERROR, true);
  err = curl_easy_perform(easy_handle.get());
  close(fp);

  if (err == CURLE_HTTP_RETURNED_ERROR) {
    // http error (error code >= 400)
    // verbose mode will display the details
    remove((root_ / path).c_str());
    return false;
  } else if (err != CURLE_OK) {
    // other unexpected error
    char *last_url = nullptr;
    curl_easy_getinfo(easy_handle.get(), CURLINFO_EFFECTIVE_URL, &last_url);
    LOG_ERROR << "Failed to get object:" << curl_easy_strerror(err);
    if (last_url != nullptr) {
      LOG_ERROR << "Url: " << last_url;
    }
    remove((root_ / path).c_str());
    return false;
  }

  return true;
}

OSTreeObject::ptr OSTreeHttpRepo::GetObject(const OSTreeHash hash, const OstreeObjectType type) const {
  otable::const_iterator it;
  it = ObjectTable.find(hash);
  if (it != ObjectTable.end()) {
    return it->second;
  }

  const std::string exts[] = {".filez", ".dirtree", ".dirmeta", ".commit"};
  const std::string objpath = hash.string().insert(2, 1, '/');
  OSTreeObject::ptr object;
  int ext_index = -1;
  switch (type) {
    case OstreeObjectType::OSTREE_OBJECT_TYPE_FILE:
      ext_index = 0;
      break;
    case OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_TREE:
      ext_index = 1;
      break;
    case OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_META:
      ext_index = 2;
      break;
    case OstreeObjectType::OSTREE_OBJECT_TYPE_COMMIT:
      ext_index = 3;
      break;
    case OstreeObjectType::OSTREE_OBJECT_TYPE_UNKNOWN:
    default:
      break;
  }

  for (int i = 0; i < 3; ++i) {
    if (i > 0) {
      LOG_WARNING << "OSTree hash " << hash << " not found. Retrying (attempt " << i << " of 3)";
    }
    if (type != OstreeObjectType::OSTREE_OBJECT_TYPE_UNKNOWN) {
      if (CheckForObject(hash, objpath + exts[ext_index], object)) {
        return object;
      }
    } else {
      for (const std::string &ext : exts) {
        if (CheckForObject(hash, objpath + ext, object)) {
          return object;
        }
      }
    }
  }
  throw OSTreeObjectMissing(hash);
}

bool OSTreeHttpRepo::CheckForObject(const OSTreeHash &hash, const std::string &path, OSTreeObject::ptr &object) const {
  if (Get(std::string("objects/") + path)) {
    object = OSTreeObject::ptr(new OSTreeObject(*this, path));
    ObjectTable[hash] = object;
    LOG_DEBUG << "Fetched OSTree object " << path;
    return true;
  }
  return false;
}

size_t OSTreeHttpRepo::curl_handle_write(void *buffer, size_t size, size_t nmemb, void *userp) {
  return static_cast<size_t>(write(*static_cast<int *>(userp), buffer, nmemb * size));
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
