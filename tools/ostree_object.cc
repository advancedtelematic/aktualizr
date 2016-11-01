#include "ostree_object.h"

#include <assert.h>
#include <iostream>

using std::cout;
using std::string;

OSTreeObject::OSTreeObject(const std::string repo_root,
                           const std::string object_name)
    : file_path_(repo_root + "/objects/" + object_name),
      object_name_(object_name),
      refcount_(0),
      is_on_server_(OBJECT_STATE_UNKNOWN),
      http_response_(),
      curl_handle_(0),
      form_post_(0) {
  assert(boost::filesystem::is_regular_file(file_path_));
}

string OSTreeObject::Url() const { return "objects/" + object_name_; }

void OSTreeObject::MakeTestRequest(const TreehubServer &push_target,
                                   CURLM *curl_multi_handle) {
  assert(!curl_handle_);
  curl_handle_ = curl_easy_init();
  current_operation_ = OSTREE_OBJECT_PRESENCE_CHECK;

  push_target.InjectIntoCurl(Url(), curl_handle_);
  curl_easy_setopt(curl_handle_, CURLOPT_NOBODY, 1);  // HEAD

  curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION,
                   &OSTreeObject::curl_handle_write);
  curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(curl_handle_, CURLOPT_PRIVATE,
                   this);  // Used by ostree_object_from_curl
  CURLMcode err = curl_multi_add_handle(curl_multi_handle, curl_handle_);
  if (err) {
    cout << "err:" << curl_multi_strerror(err) << "\n";
  }
  refcount_++;  // Because curl now has a reference to us
}

void OSTreeObject::Upload(const TreehubServer &push_target,
                          CURLM *curl_multi_handle) {
  cout << "Uploading " << object_name_ << "\n";
  assert(!curl_handle_);
  curl_handle_ = curl_easy_init();
  current_operation_ = OSTREE_OBJECT_UPLOADING;
  // TODO error checking
  push_target.InjectIntoCurl(Url(), curl_handle_);
  curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION,
                   &OSTreeObject::curl_handle_write);
  curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, this);
  // curl_easy_setopt(curl_handle_, CURLOPT_VERBOSE, 1);

  assert(form_post_ == NULL);
  struct curl_httppost *last_item = NULL;
  curl_formadd(&form_post_, &last_item, CURLFORM_COPYNAME, "file",
               CURLFORM_FILE, file_path_.c_str(), CURLFORM_END);
  curl_easy_setopt(curl_handle_, CURLOPT_POST, 1);
  CURLcode e = curl_easy_setopt(curl_handle_, CURLOPT_HTTPPOST, form_post_);
  if (e) {
    cout << "error: " << curl_easy_strerror(e) << "\n";
  }

  curl_easy_setopt(curl_handle_, CURLOPT_PRIVATE,
                   this);  // Used by ostree_object_from_curl

  CURLMcode err = curl_multi_add_handle(curl_multi_handle, curl_handle_);
  if (err) {
    cout << "err:" << curl_multi_strerror(err) << "\n";
  }
  refcount_++;  // Because curl now has a reference to us
}

void OSTreeObject::CurlDone(CURLM *curl_multi_handle) {
  refcount_--;            // Because curl now doesn't have a reference to us
  assert(refcount_ > 0);  // At least our parent should have a reference to us

  long rescode = 0;
  curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &rescode);
  if (current_operation_ == OSTREE_OBJECT_PRESENCE_CHECK) {
    if (rescode == 200) {
      is_on_server_ = OBJECT_PRESENT;
    } else if (rescode == 404) {
      is_on_server_ = OBJECT_MISSING;
    } else {
      is_on_server_ = OBJECT_BREAKS_SERVER;
      cout << "error: " << rescode << "\n";
    }
  } else if (current_operation_ == OSTREE_OBJECT_UPLOADING) {
    // TODO: check that http_response_ matches the object hash
    curl_formfree(form_post_);
    form_post_ = NULL;
    if (rescode == 200) {
      // cout << "Upload successful\n";
      is_on_server_ = OBJECT_PRESENT;
    } else {
      cout << "Upload error: " << rescode << "\n";
      is_on_server_ = OBJECT_BREAKS_SERVER;
    }
  } else {
    assert(0);
  }
  curl_multi_remove_handle(curl_multi_handle, curl_handle_);
  curl_easy_cleanup(curl_handle_);
  curl_handle_ = 0;
}

size_t OSTreeObject::curl_handle_write(void *buffer, size_t size, size_t nmemb,
                                       void *userp) {
  OSTreeObject *that = (OSTreeObject *)userp;
  that->http_response_.write((const char *)buffer, size * nmemb);
  return size * nmemb;
}

OSTreeObject::~OSTreeObject() {
  if (curl_handle_) {
    curl_easy_cleanup(curl_handle_);
    curl_handle_ = 0;
  }
}

OSTreeObject::ptr ostree_object_from_curl(CURL *curlhandle) {
  char *p;
  curl_easy_getinfo(curlhandle, CURLINFO_PRIVATE, &p);
  assert(p);
  OSTreeObject *h = reinterpret_cast<OSTreeObject *>(p);
  return boost::intrusive_ptr<OSTreeObject>(h);
}

void intrusive_ptr_add_ref(OSTreeObject *h) { h->refcount_++; }

void intrusive_ptr_release(OSTreeObject *h) {
  if (--h->refcount_ == 0) {
    delete h;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
