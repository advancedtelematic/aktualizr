#include "ostree_ref.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <utility>

#include "logging/logging.h"
#include <libaktualizr/utils.h>

using std::string;

OSTreeRef::OSTreeRef(const OSTreeRepo &repo, const string &ref_name) : ref_name_(ref_name) {
  if (boost::filesystem::is_regular_file(repo.root() / "/refs/heads/" / ref_name)) {
    std::ifstream f((repo.root() / "/refs/heads/" / ref_name).string(), std::ios::in | std::ios::binary);

    std::istream_iterator<char> start(f);
    std::istream_iterator<char> end;
    string res(start, end);

    // Strip trailing \n
    while (!res.empty() && res[res.size() - 1] == '\n') {
      res.resize(res.size() - 1);
    }
    ref_content_ = res;
    is_valid = true;
  } else {
    is_valid = false;
  }
}

OSTreeRef::OSTreeRef(const TreehubServer &serve_repo, string ref_name)
    : is_valid(true), ref_name_(std::move(ref_name)) {
  CurlEasyWrapper curl_handle;
  serve_repo.InjectIntoCurl(Url(), curl_handle.get());
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_WRITEFUNCTION, &OSTreeRef::curl_handle_write);
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_WRITEDATA, this);
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_VERBOSE, get_curlopt_verbose());
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_FAILONERROR, true);
  CURLcode rc = curl_easy_perform(curl_handle.get());
  if (rc != CURLE_OK) {
    is_valid = false;
  }
  ref_content_ = http_response_.str();
}

void OSTreeRef::PushRef(const TreehubServer &push_target, CURL *curl_handle) const {
  assert(IsValid());

  push_target.InjectIntoCurl(Url(), curl_handle);
  curlEasySetoptWrapper(curl_handle, CURLOPT_WRITEFUNCTION, &OSTreeRef::curl_handle_write);
  curlEasySetoptWrapper(curl_handle, CURLOPT_WRITEDATA, this);
  curlEasySetoptWrapper(curl_handle, CURLOPT_PRIVATE, this);  // Used by ostree_ref_from_curl

  curlEasySetoptWrapper(curl_handle, CURLOPT_POST, 1);
  curlEasySetoptWrapper(curl_handle, CURLOPT_POSTFIELDSIZE, ref_content_.size());
  curlEasySetoptWrapper(curl_handle, CURLOPT_COPYPOSTFIELDS, ref_content_.c_str());
  curlEasySetoptWrapper(curl_handle, CURLOPT_VERBOSE, get_curlopt_verbose());
}

bool OSTreeRef::IsValid() const { return is_valid; }

string OSTreeRef::Url() const { return "refs/heads/" + ref_name_; }

OSTreeHash OSTreeRef::GetHash() const { return OSTreeHash::Parse(ref_content_); }

size_t OSTreeRef::curl_handle_write(void *buffer, size_t size, size_t nmemb, void *userp) {
  auto *that = static_cast<OSTreeRef *>(userp);
  assert(that);
  that->http_response_.write(static_cast<const char *>(buffer), static_cast<std::streamsize>(size * nmemb));
  return size * nmemb;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
