#include "ostree_http_repo.h"

#include <fcntl.h>

#include <string>

#include <boost/property_tree/ini_parser.hpp>

namespace pt = boost::property_tree;

bool OSTreeHttpRepo::LooksValid() const {
  if (FetchObject("config")) {
    pt::ptree config;
    try {
      pt::read_ini((root_ / "config").string(), config);
      if (config.get<std::string>("core.mode") != "archive-z2") {
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

bool OSTreeHttpRepo::FetchObject(const boost::filesystem::path &path) const {
  CURLcode err = CURLE_OK;
  server_->InjectIntoCurl(path.string(), easy_handle_.get());
  boost::filesystem::create_directories((root_ / path).parent_path());
  std::string filename = (root_ / path).string();
  int fp = open(filename.c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
  if (fp == -1) {
    LOG_ERROR << "Failed to open file: " << filename;
    return false;
  }
  curlEasySetoptWrapper(easy_handle_.get(), CURLOPT_WRITEDATA, &fp);
  err = curl_easy_perform(easy_handle_.get());
  close(fp);

  if (err == CURLE_HTTP_RETURNED_ERROR) {
    // http error (error code >= 400)
    // verbose mode will display the details
    remove((root_ / path).c_str());
    return false;
  } else if (err != CURLE_OK) {
    // other unexpected error
    char *last_url = nullptr;
    curl_easy_getinfo(easy_handle_.get(), CURLINFO_EFFECTIVE_URL, &last_url);
    LOG_ERROR << "Failed to get object:" << curl_easy_strerror(err);
    if (last_url != nullptr) {
      LOG_ERROR << "Url: " << last_url;
    }
    remove((root_ / path).c_str());
    return false;
  }

  return true;
}

size_t OSTreeHttpRepo::curl_handle_write(void *buffer, size_t size, size_t nmemb, void *userp) {
  return static_cast<size_t>(write(*static_cast<int *>(userp), buffer, nmemb * size));
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
