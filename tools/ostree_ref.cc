#include "ostree_ref.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

using std::string;
using std::cout;
using std::stringstream;
using std::ifstream;

OSTreeRef::OSTreeRef(const string repo_root, const string ref_name)
    : file_path_(repo_root + "/refs/heads/" + ref_name), ref_name_(ref_name) {}

void OSTreeRef::PushRef(const TreehubServer &push_target, CURL *curl_handle) {
  assert(IsValid());

  push_target.InjectIntoCurl(Url(), curl_handle);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION,
                   &OSTreeRef::curl_handle_write);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(curl_handle, CURLOPT_PRIVATE,
                   this);  // Used by ostree_ref_from_curl

  curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
  string content = RefContent();
  curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, content.size());
  curl_easy_setopt(curl_handle, CURLOPT_COPYPOSTFIELDS, content.c_str());
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0);
}

bool OSTreeRef::IsValid() const {
  return boost::filesystem::is_regular_file(file_path_);
}

string OSTreeRef::Url() const { return "refs/heads/" + ref_name_; }

string OSTreeRef::RefContent() const {
  std::ifstream f(file_path_.native().c_str(), std::ios::in | std::ios::binary);

  std::istream_iterator<char> start(f);
  std::istream_iterator<char> end;
  string res(start, end);

  // Strip trailing \n
  while (!res.empty() && res[res.size() - 1] == '\n') {
    res.resize(res.size() - 1);
  }

  return res;
}

size_t OSTreeRef::curl_handle_write(void *buffer, size_t size, size_t nmemb,
                                    void *userp) {
  OSTreeRef *that = (OSTreeRef *)userp;
  assert(that);
  that->http_response_.write((const char *)buffer, size * nmemb);
  return size * nmemb;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
