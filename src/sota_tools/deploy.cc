#include "deploy.h"

#include <boost/filesystem.hpp>
#include <boost/intrusive_ptr.hpp>

#include "authenticate.h"
#include "logging/logging.h"
#include "ostree_object.h"
#include "rate_controller.h"
#include "request_pool.h"
#include "treehub_server.h"
#include "utilities/utils.h"

bool CheckPoolState(const OSTreeObject::ptr &root_object, const RequestPool &request_pool) {
  switch (request_pool.run_mode()) {
    case RunMode::kWalkTree:
    case RunMode::kPushTree:
      return !request_pool.is_stopped() && !request_pool.is_idle();
    case RunMode::kDefault:
    case RunMode::kDryRun:
    default:
      return root_object->is_on_server() != PresenceOnServer::kObjectPresent && !request_pool.is_stopped();
  }
}

bool UploadToTreehub(const OSTreeRepo::ptr &src_repo, TreehubServer &push_server, const OSTreeHash &ostree_commit,
                     const RunMode mode, const int max_curl_requests) {
  assert(max_curl_requests > 0);

  OSTreeObject::ptr root_object;
  try {
    root_object = src_repo->GetObject(ostree_commit, OstreeObjectType::OSTREE_OBJECT_TYPE_COMMIT);
  } catch (const OSTreeObjectMissing &error) {
    LOG_FATAL << "OSTree commit " << ostree_commit << " was not found in src repository";
    return false;
  }

  RequestPool request_pool(push_server, max_curl_requests, mode);

  // Add commit object to the queue.
  request_pool.AddQuery(root_object);

  // Main curl event loop.
  // request_pool takes care of holding number of outstanding requests below.
  // OSTreeObject::CurlDone() adds new requests to the pool and stops the pool
  // on error.
  do {
    request_pool.Loop();
  } while (CheckPoolState(root_object, request_pool));

  if (root_object->is_on_server() == PresenceOnServer::kObjectPresent) {
    if (mode == RunMode::kDefault || mode == RunMode::kPushTree) {
      LOG_INFO << "Upload to Treehub complete after " << request_pool.head_requests_made() << " HEAD requests and "
               << request_pool.put_requests_made() << " PUT requests.";
      LOG_INFO << "Total size of uploaded objects: " << request_pool.total_object_size() << " bytes.";
    } else {
      LOG_INFO << "Dry run. No objects uploaded.";
    }
  } else {
    LOG_ERROR << "One or more errors while pushing";
  }

  return root_object->is_on_server() == PresenceOnServer::kObjectPresent;
}

bool OfflineSignRepo(const ServerCredentials &push_credentials, const std::string &name, const OSTreeHash &hash,
                     const std::string &hardwareids) {
  const boost::filesystem::path local_repo{"./tuf/aktualizr"};

  // OTA-682: Do NOT keep the local tuf directory around in case the user tries
  // a different set of push credentials.
  if (boost::filesystem::is_directory(local_repo)) {
    boost::filesystem::remove_all(local_repo);
  }

  std::string init_cmd("garage-sign init --repo aktualizr --credentials ");
  if (system((init_cmd + push_credentials.GetPathOnDisk().string()).c_str()) != 0) {
    LOG_ERROR << "Could not initilaize tuf repo for sign";
    return false;
  }

  if (system("garage-sign targets  pull --repo aktualizr") != 0) {
    LOG_ERROR << "Could not pull targets";
    return false;
  }

  std::string cmd("garage-sign targets add --repo aktualizr --format OSTREE --length 0");
  cmd += " --name " + name + " --version " + hash.string() + " --sha256 " + hash.string();
  cmd += " --hardwareids " + hardwareids;
  if (system(cmd.c_str()) != 0) {
    LOG_ERROR << "Could not add targets";
    return false;
  }

  LOG_INFO << "Signing...\n";
  if (system("garage-sign targets  sign --key-name targets --repo aktualizr") != 0) {
    LOG_ERROR << "Could not sign targets";
    return false;
  }
  if (system("garage-sign targets  push --repo aktualizr") != 0) {
    LOG_ERROR << "Could not push signed repo";
    return false;
  }

  boost::filesystem::remove_all(local_repo);
  LOG_INFO << "Success";
  return true;
}

bool PushRootRef(const TreehubServer &push_server, const OSTreeRef &ref) {
  CurlEasyWrapper easy_handle;
  curlEasySetoptWrapper(easy_handle.get(), CURLOPT_VERBOSE, get_curlopt_verbose());
  ref.PushRef(push_server, easy_handle.get());
  CURLcode err = curl_easy_perform(easy_handle.get());
  if (err != 0U) {
    LOG_ERROR << "Error pushing root ref: " << curl_easy_strerror(err);
    return false;
  }
  long rescode;  // NOLINT(google-runtime-int)
  curl_easy_getinfo(easy_handle.get(), CURLINFO_RESPONSE_CODE, &rescode);
  if (rescode < 200 || rescode >= 400) {
    LOG_ERROR << "Error pushing root ref, got " << rescode << " HTTP response";
    return false;
  }

  return true;
}
