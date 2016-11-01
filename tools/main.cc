#include <boost/intrusive_ptr.hpp>
#include <boost/program_options.hpp>
#include <curl/curl.h>
#include <iostream>

#include "ostree_object.h"
#include "ostree_ref.h"
#include "ostree_repo.h"

namespace po = boost::program_options;
using std::cout;
using std::string;
using std::list;

const int kCurlTimeoutms = 10000;
const int kMaxCurlRequests = 30;

const string kBaseUrl =
    "https://treehub-staging.gw.prod01.advancedtelematic.com/api/v1/";
const string kPassword = "quochai1ech5oot5gaeJaifooqu6Saew";

int main(int argc, char **argv) {
  cout << "Garage push\n";

  string repo_path;
  string ref;
  TreehubServer push_target;

  po::options_description desc("Allowed options");
  // clang-format off
  desc.add_options() 
    ("help", "produce a help message")
    ("repo,C", po::value<string>(&repo_path)->required(), "location of ostree repo")
    ("ref,r", po::value<string>(&ref)->required(), "ref to push")
    ("user,u", po::value<string>(&push_target.username)->required(), "Username")
    ("password", po::value<string>(&push_target.password) ->default_value(kPassword), "Password")
    ("url", po::value<string>(&push_target.root_url)->default_value(kBaseUrl), "Treehub URL");
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

  OSTreeRef ostree_ref(repo.Ref(ref));

  if (!ostree_ref.IsValid()) {
    cout << "Ref " << ref << " was not found in repository " << repo_path
         << "\n";
    return EXIT_FAILURE;
  }

  list<OSTreeObject::ptr> work_queue;

  repo.FindAllObjects(&work_queue);

  cout << "Found " << work_queue.size() << " objects\n";

  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURLM *multi = curl_multi_init();

  int curl_requests_running = 0;

  for (; !work_queue.empty() && curl_requests_running < kMaxCurlRequests;
       curl_requests_running++) {
    OSTreeObject::ptr n = work_queue.front();
    work_queue.pop_front();
    n->MakeTestRequest(push_target, multi);
  }

  int present_already = 0;
  int uploaded = 0;
  do {
    fd_set fdread, fdwrite, fdexcept;
    int maxfd = 0;
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcept);
    long timeoutms = 0;
    curl_multi_timeout(multi, &timeoutms);
    struct timeval timeout;
    timeout.tv_sec = timeoutms / 1000;
    timeout.tv_usec = 1000 * (timeoutms % 1000);
    curl_multi_fdset(multi, &fdread, &fdwrite, &fdexcept, &maxfd);
    select(maxfd + 1, &fdread, &fdwrite, &fdexcept,
           timeoutms == -1 ? NULL : &timeout);

    CURLMcode mc = curl_multi_perform(multi, &curl_requests_running);

    if (mc != CURLM_OK) {
      std::cerr << "curl_multi failed with error" << mc << "\n";
      break;
    }

    int msgs_in_queue;
    do {
      CURLMsg *msg = curl_multi_info_read(multi, &msgs_in_queue);
      if (msg && msg->msg == CURLMSG_DONE) {
        OSTreeObject::ptr h = ostree_object_from_curl(msg->easy_handle);
        h->CurlDone(multi);
        switch (h->is_on_server()) {
          case OBJECT_MISSING:
            h->Upload(push_target, multi);
            curl_requests_running++;
            uploaded++;
            break;
          case OBJECT_PRESENT:
            present_already++;
            break;
          case OBJECT_STATE_UNKNOWN:
          case OBJECT_BREAKS_SERVER:
          default:
            cout << "Surprise state:" << h->is_on_server() << "\n";
            break;
        }
      }
    } while (msgs_in_queue > 0);

    while (curl_requests_running < kMaxCurlRequests && !work_queue.empty()) {
      OSTreeObject::ptr h = work_queue.front();
      work_queue.pop_front();
      h->MakeTestRequest(push_target, multi);
      curl_requests_running++;
    }
  } while (curl_requests_running > 0 || !work_queue.empty());

  cout << "Uploaded " << uploaded << " objects\n";
  curl_multi_cleanup(multi);

  // Push ref

  CURL *easy_handle = curl_easy_init();
  ostree_ref.PushRef(push_target, easy_handle);
  CURLcode err = curl_easy_perform(easy_handle);
  if (err) {
    cout << "Error pushing root ref:" << curl_easy_strerror(err) << "\n";
  }
  curl_easy_cleanup(easy_handle);
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
