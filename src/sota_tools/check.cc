#include <curl/curl.h>

#include "authenticate.h"
#include "check.h"
#include "garage_common.h"
#include "json/json.h"
#include "logging/logging.h"
#include "ostree_http_repo.h"
#include "ostree_object.h"
#include "rate_controller.h"
#include "request_pool.h"
#include "treehub_server.h"
#include "utilities/types.h"
#include "utilities/utils.h"

// helper function to download data to a string
static size_t writeString(void *contents, size_t size, size_t nmemb, void *userp) {
  assert(userp);
  // append the writeback data to the provided string
  (static_cast<std::string *>(userp))->append(static_cast<char *>(contents), size * nmemb);

  // return size of written data
  return size * nmemb;
}

int CheckRefValid(TreehubServer &treehub, const std::string &ref, RunMode mode, int max_curl_requests,
                  const boost::filesystem::path &tree_dir) {
  // Check if the ref is present on treehub. The traditional use case is that it
  // should be a commit object, but we allow walking the tree given any OSTree
  // ref.
  CurlEasyWrapper curl;
  if (curl.get() == nullptr) {
    LOG_FATAL << "Error initializing curl";
    return EXIT_FAILURE;
  }
  curlEasySetoptWrapper(curl.get(), CURLOPT_VERBOSE, get_curlopt_verbose());
  curlEasySetoptWrapper(curl.get(), CURLOPT_NOBODY, 1L);  // HEAD

  treehub.InjectIntoCurl("objects/" + ref.substr(0, 2) + "/" + ref.substr(2) + ".commit", curl.get());

  CURLcode result = curl_easy_perform(curl.get());
  if (result != CURLE_OK) {
    LOG_FATAL << "Error connecting to treehub: " << result << ": " << curl_easy_strerror(result);
    return EXIT_FAILURE;
  }

  long http_code;  // NOLINT(google-runtime-int)
  OstreeObjectType type = OstreeObjectType::OSTREE_OBJECT_TYPE_COMMIT;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code == 404) {
    if (mode != RunMode::kWalkTree) {
      LOG_FATAL << "OSTree commit " << ref << " is missing in treehub";
      return EXIT_FAILURE;
    } else {
      type = OstreeObjectType::OSTREE_OBJECT_TYPE_UNKNOWN;
    }
  } else if (http_code != 200) {
    LOG_FATAL << "Error " << http_code << " getting OSTree ref " << ref << " from treehub";
    return EXIT_FAILURE;
  }
  if (mode != RunMode::kWalkTree) {
    LOG_INFO << "OSTree commit " << ref << " is found on treehub";
  }

  if (mode == RunMode::kWalkTree) {
    // Walk the entire tree and check for all objects.
    OSTreeHttpRepo dest_repo(&treehub, tree_dir);
    OSTreeHash hash = OSTreeHash::Parse(ref);
    OSTreeObject::ptr input_object = dest_repo.GetObject(hash, type);

    RequestPool request_pool(treehub, max_curl_requests, mode);

    // Add input object to the queue.
    request_pool.AddQuery(input_object);

    // Main curl event loop.
    // request_pool takes care of holding number of outstanding requests below.
    // OSTreeObject::CurlDone() adds new requests to the pool and stops the pool
    // on error.
    do {
      request_pool.Loop();
    } while (!request_pool.is_idle() && !request_pool.is_stopped());

    if (input_object->is_on_server() == PresenceOnServer::kObjectPresent) {
      LOG_INFO << "Dry run. No objects uploaded.";
    } else {
      LOG_ERROR << "One or more errors while pushing";
    }
  }

  // If we have a commit object, check if the ref is present in targets.json.
  if (type == OstreeObjectType::OSTREE_OBJECT_TYPE_COMMIT) {
    curlEasySetoptWrapper(curl.get(), CURLOPT_VERBOSE, get_curlopt_verbose());
    curlEasySetoptWrapper(curl.get(), CURLOPT_HTTPGET, 1L);
    curlEasySetoptWrapper(curl.get(), CURLOPT_NOBODY, 0L);
    treehub.InjectIntoCurl("/api/v1/user_repo/targets.json", curl.get(), true);

    std::string targets_str;
    curlEasySetoptWrapper(curl.get(), CURLOPT_WRITEFUNCTION, writeString);
    curlEasySetoptWrapper(curl.get(), CURLOPT_WRITEDATA, static_cast<void *>(&targets_str));
    result = curl_easy_perform(curl.get());

    if (result != CURLE_OK) {
      LOG_FATAL << "Error connecting to TUF repo: " << result << ": " << curl_easy_strerror(result);
      return EXIT_FAILURE;
    }

    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
      LOG_FATAL << "Error " << http_code << " getting targets.json from TUF repo: " << targets_str;
      return EXIT_FAILURE;
    }

    Json::Value targets_json = Utils::parseJSON(targets_str);
    std::string expiry_time_str = targets_json["signed"]["expires"].asString();
    TimeStamp timestamp(expiry_time_str);

    if (timestamp.IsExpiredAt(TimeStamp::Now())) {
      LOG_FATAL << "targets.json has been expired.";
      return EXIT_FAILURE;
    }

    Json::Value target_list = targets_json["signed"]["targets"];
    for (auto t_it = target_list.begin(); t_it != target_list.end(); t_it++) {
      if ((*t_it)["hashes"]["sha256"].asString() == ref) {
        LOG_INFO << "OSTree commit " << ref << " is found in targets.json";
        return EXIT_SUCCESS;
      }
    }
    LOG_FATAL << "OSTree ref " << ref << " was not found in targets.json";
    return EXIT_FAILURE;
  } else {
    LOG_INFO << "OSTree ref " << ref << " is not a commit object. Skipping targets.json check.";
    return EXIT_SUCCESS;
  }
}
