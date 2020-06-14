#include "ostree_object.h"

#include <assert.h>

#include <sys/stat.h>
#include <cstring>
#include <iostream>

#include <glib.h>

#include "logging/logging.h"
#include "ostree_repo.h"
#include "request_pool.h"
#include <libaktualizr/utils.h>

using std::string;

OSTreeObject::OSTreeObject(const OSTreeRepo &repo, const std::string &object_name)
    : file_path_(repo.root() / "/objects/" / object_name),
      object_name_(object_name),
      repo_(repo),
      refcount_(0),
      is_on_server_(PresenceOnServer::kObjectStateUnknown),
      curl_handle_(nullptr),
      fd_(nullptr) {
  if (!boost::filesystem::is_regular_file(file_path_)) {
    throw std::runtime_error(file_path_.native() + " is not a valid OSTree object.");
  }
}

OSTreeObject::~OSTreeObject() {
  if (curl_handle_ != nullptr) {
    curl_easy_cleanup(curl_handle_);
    curl_handle_ = nullptr;
  }
}

void OSTreeObject::AddParent(OSTreeObject *parent, std::list<OSTreeObject::ptr>::iterator parent_it) {
  parentref par;

  par.first = parent;
  par.second = parent_it;
  parents_.push_back(par);
}

void OSTreeObject::ChildNotify(std::list<OSTreeObject::ptr>::iterator child_it) {
  assert((*child_it)->is_on_server() == PresenceOnServer::kObjectPresent);
  children_.erase(child_it);
}

void OSTreeObject::NotifyParents(RequestPool &pool) {
  assert(is_on_server_ == PresenceOnServer::kObjectPresent);

  for (parentref parent : parents_) {
    parent.first->ChildNotify(parent.second);
    if (parent.first->children_ready()) {
      pool.AddUpload(parent.first);
    }
  }
}

void OSTreeObject::AppendChild(const OSTreeObject::ptr &child) {
  // the child could be already queried/uploaded by another parent
  if (child->is_on_server() == PresenceOnServer::kObjectPresent) {
    return;
  }

  children_.push_back(child);
  auto last = children_.end();
  last--;
  child->AddParent(this, last);
}

// Can throw OSTreeObjectMissing if the repo is corrupt
void OSTreeObject::PopulateChildren() {
  const boost::filesystem::path ext = file_path_.extension();
  const GVariantType *content_type;
  bool is_commit;

  // variant types are borrowed from libostree/ostree-core.h,
  // but we don't want to create dependency on it
  if (ext.compare(".commit") == 0) {
    content_type = G_VARIANT_TYPE("(a{sv}aya(say)sstayay)");
    is_commit = true;
  } else if (ext.compare(".dirtree") == 0) {
    content_type = G_VARIANT_TYPE("(a(say)a(sayay))");
    is_commit = false;
  } else {
    return;
  }

  GError *gerror = nullptr;
  GMappedFile *mfile = g_mapped_file_new(file_path_.c_str(), FALSE, &gerror);

  if (mfile == nullptr) {
    throw std::runtime_error("Failed to map metadata file " + file_path_.native());
  }

  GVariant *contents =
      g_variant_new_from_data(content_type, g_mapped_file_get_contents(mfile), g_mapped_file_get_length(mfile), TRUE,
                              reinterpret_cast<GDestroyNotify>(g_mapped_file_unref), mfile);
  g_variant_ref_sink(contents);

  if (is_commit) {
    // * - ay - Root tree contents
    GVariant *content_csum_variant = nullptr;
    g_variant_get_child(contents, 6, "@ay", &content_csum_variant);

    gsize n_elts;
    const auto *csum = static_cast<const uint8_t *>(g_variant_get_fixed_array(content_csum_variant, &n_elts, 1));
    assert(n_elts == 32);
    AppendChild(repo_.GetObject(csum, OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_TREE));

    // * - ay - Root tree metadata
    GVariant *meta_csum_variant = nullptr;
    g_variant_get_child(contents, 7, "@ay", &meta_csum_variant);
    csum = static_cast<const uint8_t *>(g_variant_get_fixed_array(meta_csum_variant, &n_elts, 1));
    assert(n_elts == 32);
    AppendChild(repo_.GetObject(csum, OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_META));

    g_variant_unref(meta_csum_variant);
    g_variant_unref(content_csum_variant);
  } else {
    GVariant *files_variant = nullptr;
    GVariant *dirs_variant = nullptr;

    files_variant = g_variant_get_child_value(contents, 0);
    dirs_variant = g_variant_get_child_value(contents, 1);

    gsize nfiles = g_variant_n_children(files_variant);
    gsize ndirs = g_variant_n_children(dirs_variant);

    // * - a(say) - array of (filename, checksum) for files
    for (gsize i = 0; i < nfiles; i++) {
      GVariant *csum_variant = nullptr;
      const char *fname = nullptr;

      g_variant_get_child(files_variant, i, "(&s@ay)", &fname, &csum_variant);
      gsize n_elts;
      const auto *csum = static_cast<const uint8_t *>(g_variant_get_fixed_array(csum_variant, &n_elts, 1));
      assert(n_elts == 32);
      AppendChild(repo_.GetObject(csum, OstreeObjectType::OSTREE_OBJECT_TYPE_FILE));

      g_variant_unref(csum_variant);
    }

    // * - a(sayay) - array of (dirname, tree_checksum, meta_checksum) for directories
    for (gsize i = 0; i < ndirs; i++) {
      GVariant *content_csum_variant = nullptr;
      GVariant *meta_csum_variant = nullptr;
      const char *fname = nullptr;
      g_variant_get_child(dirs_variant, i, "(&s@ay@ay)", &fname, &content_csum_variant, &meta_csum_variant);
      gsize n_elts;
      // First the .dirtree:
      const auto *csum = static_cast<const uint8_t *>(g_variant_get_fixed_array(content_csum_variant, &n_elts, 1));
      assert(n_elts == 32);
      AppendChild(repo_.GetObject(csum, OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_TREE));

      // Then the .dirmeta:
      csum = static_cast<const uint8_t *>(g_variant_get_fixed_array(meta_csum_variant, &n_elts, 1));
      assert(n_elts == 32);
      AppendChild(repo_.GetObject(csum, OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_META));

      g_variant_unref(meta_csum_variant);
      g_variant_unref(content_csum_variant);
    }

    g_variant_unref(dirs_variant);
    g_variant_unref(files_variant);
  }
  g_variant_unref(contents);
}

void OSTreeObject::QueryChildren(RequestPool &pool) {
  for (const OSTreeObject::ptr &child : children_) {
    if (child->is_on_server() == PresenceOnServer::kObjectStateUnknown) {
      pool.AddQuery(child);
    }
  }
}

string OSTreeObject::Url() const { return "objects/" + object_name_; }

void OSTreeObject::MakeTestRequest(const TreehubServer &push_target, CURLM *curl_multi_handle) {
  assert(!curl_handle_);
  curl_handle_ = curl_easy_init();
  if (curl_handle_ == nullptr) {
    throw std::runtime_error("Could not initialize curl handle");
  }
  curlEasySetoptWrapper(curl_handle_, CURLOPT_VERBOSE, get_curlopt_verbose());
  current_operation_ = CurrentOp::kOstreeObjectPresenceCheck;

  push_target.InjectIntoCurl(Url(), curl_handle_);
  curlEasySetoptWrapper(curl_handle_, CURLOPT_NOBODY, 1L);  // HEAD

  curlEasySetoptWrapper(curl_handle_, CURLOPT_USERAGENT, Utils::getUserAgent());
  curlEasySetoptWrapper(curl_handle_, CURLOPT_WRITEFUNCTION, &OSTreeObject::curl_handle_write);
  curlEasySetoptWrapper(curl_handle_, CURLOPT_WRITEDATA, this);
  curlEasySetoptWrapper(curl_handle_, CURLOPT_PRIVATE, this);  // Used by ostree_object_from_curl
  http_response_.str("");                                      // Empty the response buffer

  const CURLMcode err = curl_multi_add_handle(curl_multi_handle, curl_handle_);
  if (err != 0) {
    LOG_ERROR << "err:" << curl_multi_strerror(err);
  }
  refcount_++;  // Because curl now has a reference to us
  request_start_time_ = std::chrono::steady_clock::now();
}

void OSTreeObject::Upload(TreehubServer &push_target, CURLM *curl_multi_handle, const RunMode mode) {
  if (mode == RunMode::kDefault || mode == RunMode::kPushTree) {
    LOG_INFO << "Uploading " << object_name_;
  } else {
    LOG_INFO << "Would upload " << object_name_;
    is_on_server_ = PresenceOnServer::kObjectPresent;
    return;
  }
  assert(!curl_handle_);

  curl_handle_ = curl_easy_init();
  if (curl_handle_ == nullptr) {
    throw std::runtime_error("Could not initialize curl handle");
  }
  curlEasySetoptWrapper(curl_handle_, CURLOPT_VERBOSE, get_curlopt_verbose());
  current_operation_ = CurrentOp::kOstreeObjectUploading;
  push_target.SetContentType("Content-Type: application/octet-stream");
  push_target.InjectIntoCurl(Url(), curl_handle_);
  curlEasySetoptWrapper(curl_handle_, CURLOPT_USERAGENT, Utils::getUserAgent());
  curlEasySetoptWrapper(curl_handle_, CURLOPT_WRITEFUNCTION, &OSTreeObject::curl_handle_write);
  curlEasySetoptWrapper(curl_handle_, CURLOPT_WRITEDATA, this);
  http_response_.str("");  // Empty the response buffer

  struct stat file_info {};
  fd_ = fopen(file_path_.c_str(), "rb");
  if (fd_ == nullptr) {
    throw std::runtime_error("could not open file to be uploaded");
  } else {
    if (stat(file_path_.c_str(), &file_info) < 0) {
      throw std::runtime_error("Could not get file information");
    }
  }
  curlEasySetoptWrapper(curl_handle_, CURLOPT_READDATA, fd_);
  curlEasySetoptWrapper(curl_handle_, CURLOPT_POSTFIELDSIZE, file_info.st_size);
  curlEasySetoptWrapper(curl_handle_, CURLOPT_POST, 1);

  curlEasySetoptWrapper(curl_handle_, CURLOPT_PRIVATE, this);  // Used by ostree_object_from_curl
  const CURLMcode err = curl_multi_add_handle(curl_multi_handle, curl_handle_);
  if (err != 0) {
    LOG_ERROR << "curl_multi_add_handle error:" << curl_multi_strerror(err);
    return;
  }
  refcount_++;  // Because curl now has a reference to us
  request_start_time_ = std::chrono::steady_clock::now();
}

void OSTreeObject::CheckChildren(RequestPool &pool, const long rescode) {  // NOLINT(google-runtime-int)
  try {
    PopulateChildren();
    LOG_TRACE << "Children of " << object_name_ << ": " << children_.size();
    if (children_ready()) {
      if (rescode != 200) {
        pool.AddUpload(this);
      }
    } else {
      QueryChildren(pool);
    }
  } catch (const OSTreeObjectMissing &error) {
    LOG_ERROR << "Source OSTree repo does not contain object " << error.missing_object();
    pool.Abort();
  }
}

void OSTreeObject::PresenceError(RequestPool &pool, const int64_t rescode) {
  is_on_server_ = PresenceOnServer::kObjectStateUnknown;
  LOG_WARNING << "OSTree query reported an error code: " << rescode << " retrying...";
  LOG_DEBUG << "Http response code:" << rescode;
  LOG_DEBUG << http_response_.str();
  last_operation_result_ = ServerResponse::kTemporaryFailure;
  pool.AddQuery(this);
}

void OSTreeObject::UploadError(RequestPool &pool, const int64_t rescode) {
  LOG_WARNING << "OSTree upload reported an error code:" << rescode << " retrying...";
  LOG_DEBUG << "Http response code:" << rescode;
  LOG_DEBUG << http_response_.str();
  is_on_server_ = PresenceOnServer::kObjectMissing;
  last_operation_result_ = ServerResponse::kTemporaryFailure;
  pool.AddUpload(this);
}

void OSTreeObject::CurlDone(CURLM *curl_multi_handle, RequestPool &pool) {
  refcount_--;            // Because curl now doesn't have a reference to us
  assert(refcount_ > 0);  // At least our parent should have a reference to us

  char *url = nullptr;
  curl_easy_getinfo(curl_handle_, CURLINFO_EFFECTIVE_URL, &url);
  long rescode = 0;  // NOLINT(google-runtime-int)
  curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &rescode);
  if (current_operation_ == CurrentOp::kOstreeObjectPresenceCheck) {
    // Sanity-check the handle's URL to make sure it contains the expected
    // object hash.
    if (url == nullptr || strstr(url, object_name_.c_str()) == nullptr) {
      PresenceError(pool, rescode);
    } else if (rescode == 200) {
      LOG_INFO << "Already present: " << object_name_;
      is_on_server_ = PresenceOnServer::kObjectPresent;
      last_operation_result_ = ServerResponse::kOk;
      if (pool.run_mode() == RunMode::kWalkTree || pool.run_mode() == RunMode::kPushTree) {
        CheckChildren(pool, rescode);
      } else {
        NotifyParents(pool);
      }
    } else if (rescode == 404) {
      is_on_server_ = PresenceOnServer::kObjectMissing;
      last_operation_result_ = ServerResponse::kOk;
      CheckChildren(pool, rescode);
    } else {
      PresenceError(pool, rescode);
    }

  } else if (current_operation_ == CurrentOp::kOstreeObjectUploading) {
    // Sanity-check the handle's URL to make sure it contains the expected
    // object hash.
    if (url == nullptr || strstr(url, object_name_.c_str()) == nullptr) {
      UploadError(pool, rescode);
    } else if (rescode == 204) {
      LOG_TRACE << "OSTree upload successful";
      is_on_server_ = PresenceOnServer::kObjectPresent;
      last_operation_result_ = ServerResponse::kOk;
      NotifyParents(pool);
    } else if (rescode == 409) {
      LOG_DEBUG << "OSTree upload reported a 409 Conflict, possibly due to concurrent uploads";
      is_on_server_ = PresenceOnServer::kObjectPresent;
      last_operation_result_ = ServerResponse::kOk;
      NotifyParents(pool);
    } else {
      UploadError(pool, rescode);
    }
    fclose(fd_);
  } else {
    LOG_ERROR << "Unknown operation: " << static_cast<int>(current_operation_);
    assert(0);
  }
  curl_multi_remove_handle(curl_multi_handle, curl_handle_);
  curl_easy_cleanup(curl_handle_);
  curl_handle_ = nullptr;
}

size_t OSTreeObject::curl_handle_write(void *buffer, size_t size, size_t nmemb, void *userp) {
  auto *that = static_cast<OSTreeObject *>(userp);
  that->http_response_.write(static_cast<const char *>(buffer), static_cast<std::streamsize>(size * nmemb));
  return size * nmemb;
}

OSTreeObject::ptr ostree_object_from_curl(CURL *curlhandle) {
  void *p;
  curl_easy_getinfo(curlhandle, CURLINFO_PRIVATE, &p);
  assert(p);
  auto *h = static_cast<OSTreeObject *>(p);
  return boost::intrusive_ptr<OSTreeObject>(h);
}

void intrusive_ptr_add_ref(OSTreeObject *h) { h->refcount_++; }

void intrusive_ptr_release(OSTreeObject *h) {
  if (--h->refcount_ == 0) {
    delete h;
  }
}

std::ostream &operator<<(std::ostream &stream, const OSTreeObject &o) {
  stream << o.object_name_;
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const OSTreeObject::ptr &o) {
  stream << *o;
  return stream;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
