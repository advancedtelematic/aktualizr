#include <string>

#include <curl/curl.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include "json/json.h"

#include "accumulator.h"
#include "authenticate.h"
#include "garage_common.h"
#include "logging/logging.h"
#include "ostree_http_repo.h"
#include "ostree_object.h"
#include "request_pool.h"
#include "treehub_server.h"
#include "utilities/types.h"
#include "utilities/utils.h"

namespace po = boost::program_options;

// helper function to download data to a string
static size_t writeString(void *contents, size_t size, size_t nmemb, void *userp) {
  assert(userp);
  // append the writeback data to the provided string
  (static_cast<std::string *>(userp))->append(static_cast<char *>(contents), size * nmemb);

  // return size of written data
  return size * nmemb;
}

int main(int argc, char **argv) {
  logger_init();

  int verbosity;
  std::string ref;
  boost::filesystem::path credentials_path;
  std::string cacerts;
  int max_curl_requests;
  RunMode mode = RunMode::kDefault;
  po::options_description desc("garage-check command line options");
  // clang-format off
  desc.add_options()
    ("help", "print usage")
    ("version", "Current garage-check version")
    ("verbose,v", accumulator<int>(&verbosity), "Verbose logging (use twice for more information)")
    ("quiet,q", "Quiet mode")
    ("ref,r", po::value<std::string>(&ref)->required(), "refhash to check")
    ("credentials,j", po::value<boost::filesystem::path>(&credentials_path)->required(), "credentials (json or zip containing json)")
    ("cacert", po::value<std::string>(&cacerts), "override path to CA root certificates, in the same format as curl --cacert")
    ("jobs", po::value<int>(&max_curl_requests)->default_value(30), "maximum number of parallel requests (only relevant with --walk-tree)")
    ("walk-tree,w", "walk entire tree and check presence of all objects");
  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, reinterpret_cast<const char *const *>(argv), desc), vm);

    if (vm.count("help") != 0u) {
      LOG_INFO << desc;
      return EXIT_SUCCESS;
    }
    if (vm.count("version") != 0) {
      LOG_INFO << "Current garage-check version is: " << GARAGE_TOOLS_VERSION;
      exit(EXIT_SUCCESS);
    }
    po::notify(vm);
  } catch (const po::error &o) {
    LOG_ERROR << o.what();
    LOG_ERROR << desc;
    return EXIT_FAILURE;
  }

  try {
    // Configure logging
    if (verbosity == 0) {
      // 'verbose' trumps 'quiet'
      if (static_cast<int>(vm.count("quiet")) != 0) {
        logger_set_threshold(boost::log::trivial::warning);
      } else {
        logger_set_threshold(boost::log::trivial::info);
      }
    } else if (verbosity == 1) {
      logger_set_threshold(boost::log::trivial::debug);
      LOG_DEBUG << "Debug level debugging enabled";
    } else if (verbosity > 1) {
      logger_set_threshold(boost::log::trivial::trace);
      LOG_TRACE << "Trace level debugging enabled";
    } else {
      assert(0);
    }

    if (vm.count("walk-tree") != 0u) {
      mode = RunMode::kWalkTree;
    }

    TreehubServer treehub;
    if (cacerts != "") {
      if (boost::filesystem::exists(cacerts)) {
        treehub.ca_certs(cacerts);
      } else {
        LOG_FATAL << "--cacert path " << cacerts << " does not exist";
        return EXIT_FAILURE;
      }
    }

    if (max_curl_requests < 1) {
      LOG_FATAL << "--jobs must be greater than 0";
      return EXIT_FAILURE;
    }

    if (authenticate(cacerts, ServerCredentials(credentials_path), treehub) != EXIT_SUCCESS) {
      LOG_FATAL << "Authentication failed";
      return EXIT_FAILURE;
    }

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
      OSTreeHttpRepo dest_repo(&treehub);
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
      for (Json::ValueIterator t_it = target_list.begin(); t_it != target_list.end(); t_it++) {
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
  } catch (std::exception &ex) {
    LOG_ERROR << "Exception: " << ex.what();
    return EXIT_FAILURE;
  } catch (...) {
    LOG_ERROR << "Unknown exception";
    return EXIT_FAILURE;
  }
}

// vim: set tabstop=2 shiftwidth=2 expandtab:
