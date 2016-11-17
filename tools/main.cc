#include <curl/curl.h>
#include <boost/intrusive_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iomanip>
#include <iostream>

#include "auth_plus.h"
#include "ostree_object.h"
#include "ostree_ref.h"
#include "ostree_repo.h"
#include "request_pool.h"
#include "treehub_server.h"

namespace po = boost::program_options;
using std::cout;
using std::string;
using std::list;
using boost::property_tree::ptree;
using boost::property_tree::json_parser::json_parser_error;

const int kCurlTimeoutms = 10000;
const int kMaxCurlRequests = 30;

const string kBaseUrl =
    "https://treehub-staging.gw.prod01.advancedtelematic.com/api/v1/";
const string kPassword = "quochai1ech5oot5gaeJaifooqu6Saew";
const string kAuthPlusUrl = "";

int present_already = 0;
int uploaded = 0;
int errors = 0;

int authenticate(std::string filepath, TreehubServer &treehub) {
  typedef enum { AUTH_NONE, AUTH_BASIC, AUTH_PLUS } method_t;

  method_t method = AUTH_NONE;
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

    auth_method = pt.get<std::string>("auth.method", "Basic");

    if (auth_method == "Basic") {
      method = AUTH_BASIC;
      auth_user = pt.get<std::string>("auth.user", "");
      auth_password = pt.get<std::string>("auth.password", kPassword);
    } else if (auth_method == "AuthPlus") {
      method = AUTH_PLUS;
      auth_server = pt.get<std::string>("auth.server");
      client_id = pt.get<std::string>("auth.client_id", "");
      client_secret = pt.get<std::string>("auth.client_secret", "");
    } else {
      std::cerr << "Unknown authentication method " << auth_method << std::endl;
    }
    ostree_server = pt.get<std::string>("ostree.server", kBaseUrl);
  } catch (json_parser_error e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  switch (method) {
    case AUTH_BASIC: {
      // we can also set username with command line option
      if (auth_user != "") treehub.username = auth_user;
      treehub.password = auth_password;
      break;
    }

    case AUTH_PLUS: {
      AuthPlus auth_plus(auth_server, client_id, client_secret);

      if (client_id != "") {
        if (auth_plus.Authenticate() != AUTHENTICATION_SUCCESS) {
          std::cerr << "Authentication with Auth+ failed\n";
          return EXIT_FAILURE;
        } else {
          cout << "Using Auth+ authentication token\n";
          treehub.SetToken(auth_plus.token());
        }
      } else {
        cout << "Skipping Authentication\n";
      }
      break;
    }

    default: {
      cout << "Unexpected authentication method value " << method << std::endl;
      return EXIT_FAILURE;
    }
  }
  treehub.root_url = ostree_server;

  return EXIT_SUCCESS;
}

void queried_ev(RequestPool &p, OSTreeObject::ptr h) {
  switch (h->is_on_server()) {
    case OBJECT_MISSING:
      h->PopulateChildren();
      if (h->children_ready())
        p.AddUpload(h);
      else
        h->QueryChildren(p);
      break;

    case OBJECT_PRESENT:
      present_already++;
      h->NotifyParents(p);
      break;

    case OBJECT_BREAKS_SERVER:
      std::cerr << "Uploading object " << h << " failed.\n";
      p.Abort();
      errors++;
      break;
    default:
      std::cerr << "Surprise state:" << h->is_on_server() << "\n";
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
    std::cerr << "Surprise state:" << h->is_on_server() << "\n";
    p.Abort();
    errors++;
  }
}

int main(int argc, char **argv) {
  cout << "Garage push\n";

  string repo_path;
  string ref;
  TreehubServer push_target;

  string credentials_path;
  string home_path = string(getenv("HOME"));

  po::options_description desc("Allowed options");
  // clang-format off
  desc.add_options()
    ("help", "produce a help message")
    ("repo,C", po::value<string>(&repo_path)->required(), "location of ostree repo")
    ("ref,r", po::value<string>(&ref)->required(), "ref to push")
    ("user,u", po::value<string>(&push_target.username), "Username")
    ("credentials,j", po::value<string>(&credentials_path)->default_value(home_path + "/.sota_tools.json"), "Credentials")
    ("dry-run,n", "Dry Run: Check arguments and authenticate but don't upload");
  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
      cout << desc << "\n";
      return EXIT_SUCCESS;
    }

    po::notify(vm);
  } catch (const po::error &o) {
    cout << o.what() << "\n";
    cout << desc << "\n";
    return EXIT_FAILURE;
  }

  OSTreeRepo repo(repo_path);

  if (!repo.LooksValid()) {
    cout << "The OSTree repo dir " << repo_path
         << " does not appear to contain a valid OSTree repository\n";
    return EXIT_FAILURE;
  }

  OSTreeRef ostree_ref(repo, ref);

  if (!ostree_ref.IsValid()) {
    cout << "Ref " << ref << " was not found in repository " << repo_path
         << "\n";
    return EXIT_FAILURE;
  }

  uint8_t root_sha256[32];
  ostree_ref.GetHash(root_sha256);

  OSTreeObject::ptr root_object = repo.GetObject(root_sha256);

  if (!root_object) {
    cout << "Commit pointed to by " << ref << " was not found in repository "
         << repo_path << "\n";
    return EXIT_FAILURE;
  }

  if (authenticate(credentials_path, push_target) != EXIT_SUCCESS) {
    cout << "Authentication failed\n";
    return EXIT_FAILURE;
  }

  if (vm.count("dry-run")) {
    cout << "Dry run. Exiting.\n";
    return EXIT_SUCCESS;
  }

  RequestPool request_pool(push_target, kMaxCurlRequests);

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

  cout << "Uploaded " << uploaded << " objects\n";
  cout << "Already present " << present_already << " objects\n";
  // Push ref

  if (root_object->is_on_server() == OBJECT_PRESENT) {
    CURL *easy_handle = curl_easy_init();
    // curl_easy_setopt(easy_handle, CURLOPT_VERBOSE, 1L);
    ostree_ref.PushRef(push_target, easy_handle);
    CURLcode err = curl_easy_perform(easy_handle);
    if (err) {
      cout << "Error pushing root ref:" << curl_easy_strerror(err) << "\n";
      errors++;
    }
    long rescode;
    curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &rescode);
    if (rescode != 200) {
      cout << "Error pushing root ref, got " << rescode << " HTTP response\n";
      errors++;
    }
    curl_easy_cleanup(easy_handle);
  } else {
    std::cerr << "Uploading failed\n";
  }

  if (errors) {
    std::cerr << "One or more errors while pushing\n";
    return EXIT_FAILURE;
  } else {
    return EXIT_SUCCESS;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
