#include "logging.h"
#include "ostree_object.h"
#include "ostree_repo.h"
#include "request_pool.h"

#include <assert.h>
#include <glib.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::string;

OSTreeObject::OSTreeObject(const OSTreeRepo &repo,
                           const std::string object_name)
    : file_path_(repo.root() + "/objects/" + object_name),
      object_name_(object_name),
      repo_(repo),
      refcount_(0),
      is_on_server_(OBJECT_STATE_UNKNOWN),
      http_response_(),
      curl_handle_(0),
      form_post_(0) {
  assert(boost::filesystem::is_regular_file(file_path_));
}

void OSTreeObject::AddParent(OSTreeObject *parent,
                             std::list<OSTreeObject::ptr>::iterator parent_it) {
  parentref par;

  par.first = parent;
  par.second = parent_it;
  parents_.push_back(par);
}

void OSTreeObject::ChildNotify(
    std::list<OSTreeObject::ptr>::iterator child_it) {
  assert((*child_it)->is_on_server() == OBJECT_PRESENT);
  children_.erase(child_it);
}

void OSTreeObject::NotifyParents(RequestPool &pool) {
  assert(is_on_server_ == OBJECT_PRESENT);

  BOOST_FOREACH (parentref parent, parents_) {
    parent.first->ChildNotify(parent.second);
    if (parent.first->children_ready()) pool.AddUpload(parent.first);
  }
}

void OSTreeObject::AppendChild(OSTreeObject::ptr child) {
  // the child could be already queried/uploaded by another parent
  if (child->is_on_server() == OBJECT_PRESENT) return;

  children_.push_back(child);
  std::list<OSTreeObject::ptr>::iterator last = children_.end();
  last--;
  child->AddParent(this, last);
}

// Can throw OSTreeObjectMissing if the repo is corrupt
void OSTreeObject::PopulateChildren() {
  const boost::filesystem::path ext = file_path_.extension();
  const GVariantType *content_type;
  bool is_commit;

  // variant types are borrowed from libostree/ostree_core.h,
  // but we don't want to create dependency on it
  if (ext.compare(".commit") == 0) {
    content_type = G_VARIANT_TYPE("(a{sv}aya(say)sstayay)");
    is_commit = true;
  } else if (ext.compare(".dirtree") == 0) {
    content_type = G_VARIANT_TYPE("(a(say)a(sayay))");
    is_commit = false;
  } else
    return;

  GError *gerror = NULL;
  GMappedFile *mfile = g_mapped_file_new(file_path_.c_str(), FALSE, &gerror);

  if (!mfile)
    throw new std::runtime_error("Failed to map metadata file " +
                                 file_path_.native());

  GVariant *contents =
      g_variant_new_from_data(content_type, g_mapped_file_get_contents(mfile),
                              g_mapped_file_get_length(mfile), TRUE,
                              (GDestroyNotify)g_mapped_file_unref, mfile);
  g_variant_ref_sink(contents);

  if (is_commit) {
    GVariant *content_csum_variant = NULL;
    g_variant_get_child(contents, 6, "@ay", &content_csum_variant);

    gsize n_elts;
    const uint8_t *csum = static_cast<const uint8_t *>(
        g_variant_get_fixed_array(content_csum_variant, &n_elts, 1));
    assert(n_elts == 32);
    AppendChild(repo_.GetObject(csum));

    GVariant *meta_csum_variant = NULL;

    g_variant_get_child(contents, 7, "@ay", &meta_csum_variant);
    csum = static_cast<const uint8_t *>(
        g_variant_get_fixed_array(meta_csum_variant, &n_elts, 1));
    assert(n_elts == 32);
    AppendChild(repo_.GetObject(csum));

    g_variant_unref(meta_csum_variant);
    g_variant_unref(content_csum_variant);
  } else {
    GVariant *files_variant = NULL;
    GVariant *dirs_variant = NULL;

    files_variant = g_variant_get_child_value(contents, 0);
    dirs_variant = g_variant_get_child_value(contents, 1);

    gsize nfiles = g_variant_n_children(files_variant);
    gsize ndirs = g_variant_n_children(dirs_variant);

    for (gsize i = 0; i < nfiles; i++) {
      GVariant *csum_variant = NULL;
      const char *fname = NULL;

      g_variant_get_child(files_variant, i, "(&s@ay)", &fname, &csum_variant);
      gsize n_elts;
      const uint8_t *csum = static_cast<const uint8_t *>(
          g_variant_get_fixed_array(csum_variant, &n_elts, 1));
      assert(n_elts == 32);
      AppendChild(repo_.GetObject(csum));

      g_variant_unref(csum_variant);
    }

    for (gsize i = 0; i < ndirs; i++) {
      GVariant *content_csum_variant = NULL;
      GVariant *meta_csum_variant = NULL;
      const char *fname = NULL;
      g_variant_get_child(dirs_variant, i, "(&s@ay@ay)", &fname,
                          &content_csum_variant, &meta_csum_variant);
      gsize n_elts;
      const uint8_t *csum = static_cast<const uint8_t *>(
          g_variant_get_fixed_array(content_csum_variant, &n_elts, 1));
      assert(n_elts == 32);
      AppendChild(repo_.GetObject(csum));

      csum = static_cast<const uint8_t *>(
          g_variant_get_fixed_array(meta_csum_variant, &n_elts, 1));
      assert(n_elts == 32);
      AppendChild(repo_.GetObject(csum));

      g_variant_unref(meta_csum_variant);
      g_variant_unref(content_csum_variant);
    }

    g_variant_unref(dirs_variant);
    g_variant_unref(files_variant);
  }
  g_variant_unref(contents);
}

void OSTreeObject::QueryChildren(RequestPool &pool) {
  BOOST_FOREACH (OSTreeObject::ptr child, children_) {
    if (child->is_on_server() == OBJECT_STATE_UNKNOWN) pool.AddQuery(child);
  }
}

string OSTreeObject::Url() const { return "objects/" + object_name_; }

void OSTreeObject::MakeTestRequest(const TreehubServer &push_target,
                                   CURLM *curl_multi_handle) {
  assert(!curl_handle_);
  curl_handle_ = curl_easy_init();
  curl_easy_setopt(curl_handle_, CURLOPT_VERBOSE, get_curlopt_verbose());
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
    LOG_ERROR << "err:" << curl_multi_strerror(err);
  }
  refcount_++;  // Because curl now has a reference to us
}

void OSTreeObject::Upload(const TreehubServer &push_target,
                          CURLM *curl_multi_handle) {
  LOG_INFO << "Uploading " << object_name_;
  assert(!curl_handle_);
  curl_handle_ = curl_easy_init();
  curl_easy_setopt(curl_handle_, CURLOPT_VERBOSE, get_curlopt_verbose());
  current_operation_ = OSTREE_OBJECT_UPLOADING;
  // TODO error checking
  push_target.InjectIntoCurl(Url(), curl_handle_);
  curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION,
                   &OSTreeObject::curl_handle_write);
  curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, this);

  assert(form_post_ == NULL);
  struct curl_httppost *last_item = NULL;
  curl_formadd(&form_post_, &last_item, CURLFORM_COPYNAME, "file",
               CURLFORM_FILE, file_path_.c_str(), CURLFORM_END);
  curl_easy_setopt(curl_handle_, CURLOPT_POST, 1);
  CURLcode e = curl_easy_setopt(curl_handle_, CURLOPT_HTTPPOST, form_post_);
  if (e) {
    LOG_ERROR << "curl_easy_setopt error: " << curl_easy_strerror(e);
  }

  curl_easy_setopt(curl_handle_, CURLOPT_PRIVATE,
                   this);  // Used by ostree_object_from_curl

  CURLMcode err = curl_multi_add_handle(curl_multi_handle, curl_handle_);
  if (err) {
    LOG_ERROR << "curl_multi_add_handle error:" << curl_multi_strerror(err);
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
      LOG_ERROR << "error: " << rescode;
    }
  } else if (current_operation_ == OSTREE_OBJECT_UPLOADING) {
    // TODO: check that http_response_ matches the object hash
    curl_formfree(form_post_);
    form_post_ = NULL;
    if (rescode == 200) {
      LOG_DEBUG << "Upload successful";
      is_on_server_ = OBJECT_PRESENT;
    } else {
      LOG_ERROR << "Upload error: " << rescode;
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

std::ostream &operator<<(std::ostream &stream, const OSTreeObject &o) {
  stream << o.object_name_;
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const OSTreeObject::ptr o) {
  stream << *o;
  return stream;
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
