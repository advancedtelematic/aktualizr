#include "deploy.h"

#include <boost/filesystem.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/make_shared.hpp>

#include "authenticate.h"
#include "logging.h"
#include "ostree_dir_repo.h"
#include "ostree_http_repo.h"
#include "ostree_object.h"
#include "ostree_ref.h"
#include "request_pool.h"
#include "treehub_server.h"

int present_already = 0;
int uploaded = 0;
int errors = 0;

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
        LOG_ERROR << "Local OSTree repo does not contain object " << error.missing_object();
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

bool copy_repo(const std::string &cacerts, const std::string &src, const std::string &dst, const std::string &ref,
               bool dryrun) {
  TreehubServer push_server;
  TreehubServer fetch_server;

  if (cacerts != "") {
    if (boost::filesystem::exists(cacerts)) {
      push_server.ca_certs(cacerts);
      fetch_server.ca_certs(cacerts);
    } else {
      LOG_FATAL << "--cacert path " << cacerts << " does not exist";
      return EXIT_FAILURE;
    }
  }

  boost::shared_ptr<OSTreeRepo> src_repo;
  if (boost::filesystem::is_regular_file(src)) {
    if (authenticate(cacerts, src, fetch_server) != EXIT_SUCCESS) {
      LOG_FATAL << "Authentication failed";
      return EXIT_FAILURE;
    }
    src_repo = boost::make_shared<OSTreeHttpRepo>(&fetch_server);
  } else {
    src_repo = boost::make_shared<OSTreeDirRepo>(src);
  }

  if (!src_repo->LooksValid()) {
    LOG_FATAL << "The OSTree src repo does not appear to contain a valid OSTree repository";
    return EXIT_FAILURE;
  }

  OSTreeRef ostree_ref(src_repo->GetRef(ref));
  if (!ostree_ref.IsValid()) {
    LOG_FATAL << "Ref " << ref << " was not found in repository " << src;
    return EXIT_FAILURE;
  }

  if (authenticate(cacerts, dst, push_server) != EXIT_SUCCESS) {
    LOG_FATAL << "Authentication failed";
    return EXIT_FAILURE;
  }

  OSTreeHash root_hash = ostree_ref.GetHash();
  OSTreeObject::ptr root_object;
  try {
    root_object = src_repo->GetObject(root_hash);
  } catch (const OSTreeObjectMissing &error) {
    LOG_FATAL << "Commit pointed to by " << ref << " was not found in src repository ";
    return EXIT_FAILURE;
  }

  if (dryrun) {
    LOG_INFO << "Dry run. Exiting.";
    return EXIT_SUCCESS;
  }

  RequestPool request_pool(push_server, 30);

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
  } while (root_object->is_on_server() != OBJECT_PRESENT && !request_pool.is_stopped());

  LOG_INFO << "Uploaded " << uploaded << " objects";
  LOG_INFO << "Already present " << present_already << " objects";
  // Push ref

  if (root_object->is_on_server() == OBJECT_PRESENT) {
    CURL *easy_handle = curl_easy_init();
    curl_easy_setopt(easy_handle, CURLOPT_VERBOSE, get_curlopt_verbose());
    ostree_ref.PushRef(push_server, easy_handle);
    CURLcode err = curl_easy_perform(easy_handle);
    if (err) {
      LOG_ERROR << "Error pushing root ref:" << curl_easy_strerror(err);
      errors++;
    }
    long rescode;
    curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &rescode);
    if (rescode != 200) {
      LOG_ERROR << "Error pushing root ref, got " << rescode << " HTTP response";
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

  return EXIT_SUCCESS;
}