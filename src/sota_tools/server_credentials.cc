#include "server_credentials.h"

#include <archive.h>
#include <archive_entry.h>
#include <boost/move/make_unique.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

using boost::optional;
using boost::property_tree::json_parser::json_parser_error;
using boost::property_tree::ptree;

const std::string kBaseUrl = "https://treehub-staging.gw.prod01.advancedtelematic.com/api/v1/";
const std::string kPassword = "quochai1ech5oot5gaeJaifooqu6Saew";

boost::movelib::unique_ptr<std::stringstream> readArchiveFile(archive *a) {
  int r;
  const char *buff = nullptr;
  boost::movelib::unique_ptr<std::stringstream> result = boost::movelib::make_unique<std::stringstream>();
  size_t size;
  int64_t offset;
  for (;;) {
    r = archive_read_data_block(a, (const void **)&buff, &size, &offset);
    if (r == ARCHIVE_EOF) {
      break;
    } else if (r != ARCHIVE_OK) {
      throw BadCredentialsArchive(archive_error_string(a));
      break;
    } else if (size > 0 && buff != nullptr) {
      result->write(buff, size);
    }
  }
  return result;
}

ServerCredentials::ServerCredentials(const boost::filesystem::path &credentials_path)
    : method_(AUTH_NONE), credentials_path_(credentials_path) {
  bool found_config = false;

  boost::movelib::unique_ptr<std::stringstream> json_stream;

  struct archive *a;
  struct archive_entry *entry;
  int r;

  /* Try reading the file as an archive of any format. If that fails, try
   * reading it as a json. */
  a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);
  r = archive_read_open_filename(a, credentials_path_.c_str(), 1024);
  if (r == ARCHIVE_OK) {
    const char *filename = NULL;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
      filename = archive_entry_pathname(entry);
      if (!strcmp(filename, "treehub.json")) {
        json_stream = readArchiveFile(a);
        found_config = true;
      } else if (strcmp(filename, "client.crt") == 0) {
        client_cert_ = readArchiveFile(a)->str();
      } else if (strcmp(filename, "client.key") == 0) {
        client_key_ = readArchiveFile(a)->str();
      } else if (strcmp(filename, "root.crt") == 0) {
        root_cert_ = readArchiveFile(a)->str();
      } else if (strcmp(filename, "tufrepo.url") == 0) {
        repo_url_ = readArchiveFile(a)->str();
      } else {
        archive_read_data_skip(a);
      }
    }
    r = archive_read_free(a);
    if (r != ARCHIVE_OK) {
      throw BadCredentialsArchive(std::string("Error closing zipped credentials file: ") + credentials_path.string());
    } else if (!found_config) {
      throw BadCredentialsContent(std::string("treehub.json not found in zipped credentials file: ") +
                                  credentials_path.string());
    }
  }

  try {
    ptree pt;

    if (found_config) {
      read_json(*json_stream, pt);
    } else {
      read_json(credentials_path.string(), pt);
    }

    if (optional<ptree &> ap_pt = pt.get_child_optional("oauth2")) {
      method_ = OAUTH2;
      auth_server_ = ap_pt->get<std::string>("server", "");
      client_id_ = ap_pt->get<std::string>("client_id", "");
      client_secret_ = ap_pt->get<std::string>("client_secret", "");
    } else if (optional<ptree &> ba_pt = pt.get_child_optional("basic_auth")) {
      method_ = AUTH_BASIC;
      auth_user_ = ba_pt->get<std::string>("user", "");
      auth_password_ = ba_pt->get<std::string>("password", kPassword);
    } else if (pt.get<bool>("certificate_auth", false)) {
      if (client_cert_.size() && client_key_.size() && root_cert_.size()) {
        method_ = CERT;
      } else {
        throw BadCredentialsContent(
            "treehub.json requires certificate authentication, but credential archive, didn't include all or "
            "some certificate files");
      }
    }
    ostree_server_ = pt.get<std::string>("ostree.server", kBaseUrl);

  } catch (json_parser_error e) {
    throw BadCredentialsJson(std::string("Unable to read ") + credentials_path.string() + " as archive or json file.");
  }
}

bool ServerCredentials::CanSignOffline() const {
  bool found_root = false;
  bool found_targets_pub = false;
  bool found_targets_sec = false;
  bool found_tufrepo_url = false;

  struct archive *a;
  struct archive_entry *entry;
  int r;

  a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);
  r = archive_read_open_filename(a, credentials_path_.c_str(), 1024);
  if (r == ARCHIVE_OK) {
    const char *filename = NULL;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
      filename = archive_entry_pathname(entry);
      if (!strcmp(filename, "root.json")) {
        found_root = true;
      } else if (!strcmp(filename, "targets.pub")) {
        found_targets_pub = true;
      } else if (!strcmp(filename, "targets.sec")) {
        found_targets_sec = true;
      } else if (!strcmp(filename, "tufrepo.url")) {
        found_tufrepo_url = true;
      } else {
        archive_read_data_skip(a);
      }
    }
    (void)archive_read_free(a);
  }
  return (found_root && found_targets_pub && found_targets_sec && found_tufrepo_url);
}
