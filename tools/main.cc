#include <boost/intrusive_ptr.hpp>
#include <boost/program_options.hpp>
#include <curl/curl.h>
#include <iostream>
#include <iomanip>

#include "auth_plus.h"
#include "ostree_object.h"
#include "ostree_ref.h"
#include "ostree_repo.h"
#include "treehub_server.h"
#include "request_pool.h"

namespace po = boost::program_options;
using std::cout;
using std::string;
using std::list;

const int kCurlTimeoutms = 10000;
const int kMaxCurlRequests = 30;

const string kBaseUrl =
    "https://treehub-staging.gw.prod01.advancedtelematic.com/api/v1/";
const string kPassword = "quochai1ech5oot5gaeJaifooqu6Saew";
const string kAuthPlusUrl = "";

int present_already = 0;
int uploaded = 0;
int errors = 0;

void queried_ev(RequestPool &p, OSTreeObject::ptr h) {
  switch (h->is_on_server()) {
    case OBJECT_MISSING:
      h->populate_children();
      if (h->children_ready())
        p.add_upload(h);
      else
        h->query_children(p);
      break;

    case OBJECT_PRESENT:
      present_already++;
      h->notify_parents(p);
      break;

    default:
      std::cerr << "Surprise state:" << h->is_on_server() << "\n";
      p.abort();
      errors++;
      break;
  }
}

void uploaded_ev(RequestPool &p, OSTreeObject::ptr h) {
  if (h->is_on_server() == OBJECT_PRESENT) {
    uploaded++;
    h->notify_parents(p);
  } else {
    std::cerr << "Surprise state:" << h->is_on_server() << "\n";
    p.abort();
    errors++;
  }
}

int main(int argc, char **argv) {
  cout << "Garage push\n";

  string repo_path;
  string ref;
  TreehubServer push_target;

  string auth_plus_server;
  string client_id;
  string client_secret;

  po::options_description desc("Allowed options");
  // clang-format off
  desc.add_options()
    ("help", "produce a help message")
    ("repo,C", po::value<string>(&repo_path)->required(), "location of ostree repo")
    ("ref,r", po::value<string>(&ref)->required(), "ref to push")
    ("user,u", po::value<string>(&push_target.username)->required(), "Username")
    ("password", po::value<string>(&push_target.password) ->default_value(kPassword), "Password")
    ("url", po::value<string>(&push_target.root_url)->default_value(kBaseUrl), "Treehub URL")
    ("auth-server", po::value<string>(&auth_plus_server), "Auth+ Server")
    ("client-id", po::value<string>(&client_id), "Client ID")
    ("client-secret", po::value<string>(&client_secret), "Client Secret")
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

  // Authenticate with Auth+
  AuthPlus auth_plus(auth_plus_server, client_id, client_secret);

  if (client_id != "") {
    if (auth_plus.Authenticate() != AUTHENTICATION_SUCCESS) {
      std::cerr << "Authentication with Auth+ failed\n";
      return EXIT_FAILURE;
    } else {
      cout << "Using Auth+ authentication token\n";
      push_target.SetToken(auth_plus.token());
    }
  } else {
    cout << "Skipping Authentication\n";
  }

  if (vm.count("dry-run")) {
    cout << "Dry run. Exiting.\n";
    return EXIT_SUCCESS;
  }

  RequestPool request_pool(push_target, kMaxCurlRequests);

  // Add commit object to the queue
  request_pool.add_query(root_object);

  // Set callbacks
  request_pool.on_query(queried_ev);
  request_pool.on_upload(uploaded_ev);

  // Main curl event loop.
  // request_pool takes care of holding number of outstanding requests below
  // kMaxCurlRequests
  // Callbacks (queried_ev and uploaded_ev) add new requests to the pool and
  // stop the pool on error

  do {
    request_pool.loop();
  } while (root_object->is_on_server() != OBJECT_PRESENT &&
           !request_pool.is_stopped());

  cout << "Uploaded " << uploaded << " objects\n";
  cout << "Already present " << present_already << " objects\n";
  // Push ref

  CURL *easy_handle = curl_easy_init();
  curl_easy_setopt(easy_handle, CURLOPT_VERBOSE, 1L);
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

  if (errors) {
    std::cerr << "One or more errors while pushing\n";
    return EXIT_FAILURE;
  } else {
    return EXIT_SUCCESS;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
