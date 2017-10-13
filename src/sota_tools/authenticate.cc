#include <archive.h>
#include <archive_entry.h>
#include <boost/optional.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <sstream>

#include "authenticate.h"
#include "logging.h"
#include "oauth2.h"

using std::string;
using boost::optional;
using boost::property_tree::ptree;
using boost::property_tree::json_parser::json_parser_error;

const string kBaseUrl = "https://treehub-staging.gw.prod01.advancedtelematic.com/api/v1/";
const string kPassword = "quochai1ech5oot5gaeJaifooqu6Saew";

enum AuthMethod { AUTH_NONE = 0, AUTH_BASIC, OAUTH2, CERT };

std::stringstream readArchiveFile(archive *a) {
  int r;
  const char *buff = nullptr;
  std::stringstream result;
  size_t size;
  int64_t offset;
  for (;;) {
    r = archive_read_data_block(a, (const void **)&buff, &size, &offset);
    if (r == ARCHIVE_EOF) {
      break;
    } else if (r != ARCHIVE_OK) {
      std::cerr << archive_error_string(a) << std::endl;
      break;
    } else if (size > 0 && buff != nullptr) {
      result.write(buff, size);
    }
  }
  return result;
}

int authenticate(const string &cacerts, string filepath, TreehubServer &treehub) {
  AuthMethod method = AUTH_NONE;
  string auth_method;
  string auth_user;
  string auth_password;
  string auth_server;
  string ostree_server;
  string client_id;
  string client_secret;
  std::stringstream json_stream;
  std::string client_cert;
  std::string client_key;
  std::string root_cert;
  bool found = false;

  struct archive *a;
  struct archive_entry *entry;
  int r;

  /* Try reading the file as an archive of any format. If that fails, try
   * reading it as a json. */
  a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);
  r = archive_read_open_filename(a, filepath.c_str(), 1024);
  if (r == ARCHIVE_OK) {
    const char *filename = NULL;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
      filename = archive_entry_pathname(entry);
      if (!strcmp(filename, "treehub.json")) {
        json_stream = readArchiveFile(a);
        found = true;
      } else if (strcmp(filename, "client.crt") == 0) {
        client_cert = readArchiveFile(a).str();
      } else if (strcmp(filename, "client.key") == 0) {
        client_key = readArchiveFile(a).str();
      } else if (strcmp(filename, "root.crt") == 0) {
        root_cert = readArchiveFile(a).str();
      } else {
        archive_read_data_skip(a);
      }
    }
    r = archive_read_free(a);
    if (r != ARCHIVE_OK) {
      std::cerr << "Error closing zipped credentials file: " << filepath << std::endl;
      return EXIT_FAILURE;
    }
  }

  try {
    ptree pt;

    if (found) {
      read_json(json_stream, pt);
    } else {
      read_json(filepath, pt);
    }

    if (optional<ptree &> ap_pt = pt.get_child_optional("oauth2")) {
      method = OAUTH2;
      auth_server = ap_pt->get<string>("server", "");
      client_id = ap_pt->get<string>("client_id", "");
      client_secret = ap_pt->get<string>("client_secret", "");
    } else if (optional<ptree &> ba_pt = pt.get_child_optional("basic_auth")) {
      method = AUTH_BASIC;
      auth_user = ba_pt->get<string>("user", "");
      auth_password = ba_pt->get<string>("password", kPassword);
    }
    ostree_server = pt.get<string>("ostree.server", kBaseUrl);

  } catch (json_parser_error e) {
    std::cerr << e.what() << std::endl;
    std::cerr << "Unable to read " << filepath << " as archive or json file." << std::endl;
    return EXIT_FAILURE;
  }

  if (method == AUTH_NONE && client_cert.size() && client_key.size()) {
    method = CERT;
  }

  switch (method) {
    case AUTH_BASIC: {
      treehub.username(auth_user);
      treehub.password(auth_password);
      break;
    }

    case OAUTH2: {
      OAuth2 oauth2(auth_server, client_id, client_secret, cacerts);

      if (client_id != "") {
        if (oauth2.Authenticate() != AUTHENTICATION_SUCCESS) {
          LOG_FATAL << "Authentication with oauth2 failed";
          return EXIT_FAILURE;
        } else {
          LOG_INFO << "Using oauth2 authentication token";
          treehub.SetToken(oauth2.token());
        }
      } else {
        LOG_INFO << "Skipping Authentication";
      }
      break;
    }
    case CERT: {
      treehub.SetCerts(root_cert, client_cert, client_key);
      break;
    }

    default: {
      LOG_FATAL << "Unexpected authentication method value " << method;
      return EXIT_FAILURE;
    }
  }
  treehub.root_url(ostree_server);

  return EXIT_SUCCESS;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
