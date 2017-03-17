#include <curl/curl.h>
#include <boost/filesystem.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iomanip>
#include <iostream>

#include "accumulator.h"
#include "logging.h"
#include "oauth2.h"
#include "ostree_object.h"
#include "ostree_ref.h"
#include "ostree_repo.h"
#include "request_pool.h"
#include "treehub_server.h"

namespace po = boost::program_options;
using std::string;
using std::list;
using boost::optional;
using boost::property_tree::ptree;
using boost::property_tree::json_parser::json_parser_error;

const string kBaseUrl =
    "https://treehub-staging.gw.prod01.advancedtelematic.com/api/v1/";
const string kPassword = "quochai1ech5oot5gaeJaifooqu6Saew";

int present_already = 0;
int uploaded = 0;
int errors = 0;

enum AuthMethod { AUTH_NONE = 0, AUTH_BASIC, OAUTH2 };

int authenticate(const string &cacerts, string filepath,
                 TreehubServer &treehub) {
  AuthMethod method = AUTH_NONE;
  std::string auth_method;
  std::string auth_user;
  std::string auth_password;
  std::string auth_server;
  std::string ostree_server;
  std::string client_id;
  std::string client_secret;

  try {
    ptree pt;

    read_json(filepath, pt);

    if (optional<ptree &> ap_pt = pt.get_child_optional("oauth2")) {
      method = OAUTH2;
      auth_server = ap_pt->get<std::string>("server", "");
      client_id = ap_pt->get<std::string>("client_id", "");
      client_secret = ap_pt->get<std::string>("client_secret", "");
    } else if (optional<ptree &> ba_pt = pt.get_child_optional("basic_auth")) {
      method = AUTH_BASIC;
      auth_user = ba_pt->get<std::string>("user", "");
      auth_password = ba_pt->get<std::string>("password", kPassword);
    } else {
      std::cerr << "Unknown authentication method " << std::endl;
    }

    ostree_server = pt.get<std::string>("ostree.server", kBaseUrl);

  } catch (json_parser_error e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
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

    default: {
      LOG_FATAL << "Unexpected authentication method value " << method;
      return EXIT_FAILURE;
    }
  }
  treehub.root_url(ostree_server);

  return EXIT_SUCCESS;
}

void queried_ev(RequestPool &p, OSTreeObject::ptr h) {
  switch (h->is_on_server()) {
    case OBJECT_MISSING:
      try {
        h->PopulateChildren();
        if (h->children_ready()) {
          p.AddUpload(h);
        } else {
          h->QueryChildren(p);
        }
      } catch (const OSTreeObjectMissing &error) {
        LOG_ERROR << "Local OSTree repo does not contain object "
                  << error.missing_object();
        p.Abort();
        errors++;
      }
      break;

    case OBJECT_PRESENT:
      present_already++;
      h->NotifyParents(p);
      break;

    case OBJECT_BREAKS_SERVER:
      LOG_ERROR << "Uploading object " << h << " failed.";
      p.Abort();
      errors++;
      break;
    default:
      LOG_ERROR << "Surprise state:" << h->is_on_server();
      p.Abort();
      errors++;
      break;
  }
}

void uploaded_ev(RequestPool &p, OSTreeObject::ptr h) {
  if (h->is_on_server() == OBJECT_PRESENT) {
    uploaded++;
    h->NotifyParents(p);
  } else {
    LOG_ERROR << "Surprise state:" << h->is_on_server();
    p.Abort();
    errors++;
  }
}

int main(int argc, char **argv) {
  logger_init();

  string repo_path;
  string ref;
  TreehubServer push_target;
  int maxCurlRequests;

  string credentials_path;
  string home_path = string(getenv("HOME"));
  string cacerts;

  int verbosity;

  po::options_description desc("Allowed options");
  // clang-format off
  desc.add_options()
    ("help", "produce a help message")
    ("verbose,v", accumulator<int>(&verbosity), "Verbose logging (use twice for more information)")
    ("quiet,q", "Quiet mode")
    ("repo,C", po::value<string>(&repo_path)->required(), "location of ostree repo")
    ("ref,r", po::value<string>(&ref)->required(), "ref to push")
    ("credentials,j", po::value<string>(&credentials_path)->default_value(home_path + "/.sota_tools.json"), "Credentials")
    ("cacert", po::value<string>(&cacerts), "Override path to CA root certificates, in the same format as curl --cacert")
    ("jobs", po::value<int>(&maxCurlRequests)->default_value(30), "Maximum number of parallel requests")
    ("dry-run,n", "Dry Run: Check arguments and authenticate but don't upload");
  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, (const char *const *)argv, desc),
              vm);

    if (vm.count("help")) {
      LOG_INFO << desc;
      return EXIT_SUCCESS;
    }

    po::notify(vm);
  } catch (const po::error &o) {
    LOG_INFO << o.what();
    LOG_INFO << desc;
    return EXIT_FAILURE;
  }

  // Configure logging
  if (verbosity == 0) {
    // 'verbose' trumps 'quiet'
    if ((int)vm.count("quiet")) {
      logger_set_threshold(boost::log::trivial::warning);
    } else {
      logger_set_threshold(boost::log::trivial::info);
    }
  } else if (verbosity == 1) {
    logger_set_threshold(boost::log::trivial::debug);
    LOG_DEBUG << "Debug level debugging enabled";
  } else if (verbosity > 1) {
    logger_set_threshold(boost::log::trivial::trace);
    LOG_TRACE << "Trace level debugging enabled";
  } else {
    assert(0);
  }

  OSTreeRepo repo(repo_path);

  if (!repo.LooksValid()) {
    LOG_FATAL << "The OSTree repo dir " << repo_path
              << " does not appear to contain a valid OSTree repository";
    return EXIT_FAILURE;
  }

  OSTreeRef ostree_ref(repo, ref);

  if (!ostree_ref.IsValid()) {
    LOG_FATAL << "Ref " << ref << " was not found in repository " << repo_path;
    return EXIT_FAILURE;
  }

  if (maxCurlRequests < 1 || 1000 < maxCurlRequests) {
    LOG_FATAL << "--jobs must in in the range 1...1000";
    return EXIT_FAILURE;
  }

  if (cacerts != "") {
    if (boost::filesystem::exists(cacerts)) {
      push_target.ca_certs(cacerts);
    } else {
      LOG_FATAL << "--cacert path " << cacerts << " does not exist";
      return EXIT_FAILURE;
    }
  }

  OSTreeHash root_hash = ostree_ref.GetHash();
  OSTreeObject::ptr root_object;
  try {
    root_object = repo.GetObject(root_hash);
  } catch (const OSTreeObjectMissing &error) {
    LOG_FATAL << "Commit pointed to by " << ref
              << " was not found in repository " << repo_path;
    return EXIT_FAILURE;
  }

  if (authenticate(cacerts, credentials_path, push_target) != EXIT_SUCCESS) {
    LOG_FATAL << "Authentication failed";
    return EXIT_FAILURE;
  }

  if (vm.count("dry-run")) {
    LOG_INFO << "Dry run. Exiting.";
    return EXIT_SUCCESS;
  }

  RequestPool request_pool(push_target, maxCurlRequests);

  // Add commit object to the queue
  request_pool.AddQuery(root_object);

  // Set callbacks
  request_pool.OnQuery(queried_ev);
  request_pool.OnUpload(uploaded_ev);

  // Main curl event loop.
  // request_pool takes care of holding number of outstanding requests below
  // kMaxCurlRequests
  // Callbacks (queried_ev and uploaded_ev) add new requests to the pool and
  // stop the pool on error

  do {
    request_pool.Loop();
  } while (root_object->is_on_server() != OBJECT_PRESENT &&
           !request_pool.is_stopped());

  LOG_INFO << "Uploaded " << uploaded << " objects";
  LOG_INFO << "Already present " << present_already << " objects";
  // Push ref

  if (root_object->is_on_server() == OBJECT_PRESENT) {
    CURL *easy_handle = curl_easy_init();
    curl_easy_setopt(easy_handle, CURLOPT_VERBOSE, get_curlopt_verbose());
    ostree_ref.PushRef(push_target, easy_handle);
    CURLcode err = curl_easy_perform(easy_handle);
    if (err) {
      LOG_ERROR << "Error pushing root ref:" << curl_easy_strerror(err);
      errors++;
    }
    long rescode;
    curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &rescode);
    if (rescode != 200) {
      LOG_ERROR << "Error pushing root ref, got " << rescode
                << " HTTP response";
      errors++;
    }
    curl_easy_cleanup(easy_handle);
  } else {
    LOG_ERROR << "Uploading failed";
  }

  if (errors) {
    LOG_ERROR << "One or more errors while pushing";
    return EXIT_FAILURE;
  } else {
    return EXIT_SUCCESS;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
