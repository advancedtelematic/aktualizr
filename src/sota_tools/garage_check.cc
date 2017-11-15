
#include <curl/curl.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>

#include "accumulator.h"
#include "authenticate.h"
#include "json/json.h"
#include "logging.h"
#include "treehub_server.h"

namespace po = boost::program_options;
using std::string;

// helper function to download data to a string
static size_t writeString(void *contents, size_t size, size_t nmemb, void *userp) {
  assert(userp);
  // append the writeback data to the provided string
  (static_cast<std::string *>(userp))->append((char *)contents, size * nmemb);

  // return size of written data
  return size * nmemb;
}

int main(int argc, char **argv) {
  logger_init();

  string repo_path;
  string ref = "uninitialized";
  string pull_cred;

  string credentials_path;
  string home_path = string(getenv("HOME"));
  string cacerts;

  int verbosity;
  po::options_description desc("Allowed options");
  // clang-format off
  desc.add_options()
    ("help", "produce a help message")
    ("verbose,v", accumulator<int>(&verbosity), "Verbose logging (use twice for more information)")
    ("quiet,q", "Quiet mode")
    ("ref,r", po::value<string>(&ref)->required(), "refhash to check")
    ("credentials,j", po::value<string>(&credentials_path)->required(), "Credentials (json or zip containing json)")
    ("cacert", po::value<string>(&cacerts), "Override path to CA root certificates, in the same format as curl --cacert");
  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, (const char *const *)argv, desc), vm);

    if (vm.count("help")) {
      LOG_INFO << desc;
      return EXIT_SUCCESS;
    }

    po::notify(vm);
  } catch (const po::error &o) {
    LOG_INFO << o.what();
    LOG_INFO << desc;
    return EXIT_FAILURE;
  }

  // Configure logging
  if (verbosity == 0) {
    // 'verbose' trumps 'quiet'
    if ((int)vm.count("quiet")) {
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

  TreehubServer treehub;
  if (cacerts != "") {
    if (boost::filesystem::exists(cacerts)) {
      treehub.ca_certs(cacerts);
    } else {
      LOG_FATAL << "--cacert path " << cacerts << " does not exist";
      return EXIT_FAILURE;
    }
  }

  if (authenticate(cacerts, credentials_path, treehub) != EXIT_SUCCESS) {
    LOG_FATAL << "Authentication failed";
    return EXIT_FAILURE;
  }

  // check if the ref is present on treehub
  CURL *curl_handle = curl_easy_init();
  if (!curl_handle) {
    LOG_FATAL << "Error initializing curl";
    return EXIT_FAILURE;
  }
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, get_curlopt_verbose());
  curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);  // HEAD

  treehub.InjectIntoCurl("objects/" + ref.substr(0, 2) + "/" + ref.substr(2) + ".commit", curl_handle);

  CURLcode result = curl_easy_perform(curl_handle);
  curl_easy_cleanup(curl_handle);
  if (result != CURLE_OK) {
    LOG_FATAL << "Error connecting to treehub: " << result << ": " << curl_easy_strerror(result);
    return EXIT_FAILURE;
  }

  long http_code;
  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code == 404) {
    LOG_FATAL << "OSTree commit " << ref << " is missing in treehub";
    return EXIT_FAILURE;
  } else if (http_code != 200) {
    LOG_FATAL << "Error " << http_code << " getting commit " << ref << " from treehub";
    return EXIT_FAILURE;
  }
  LOG_INFO << "OSTree commit " << ref << " is found on treehub";

  // check if the ref is present in targets.json
  curl_handle = curl_easy_init();
  if (!curl_handle) {
    LOG_FATAL << "Error initializing curl";
    return EXIT_FAILURE;
  }

  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, get_curlopt_verbose());
  curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L);
  treehub.InjectIntoCurl("targets.json", curl_handle, true);

  std::string targets_str;
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeString);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&targets_str);
  result = curl_easy_perform(curl_handle);
  curl_easy_cleanup(curl_handle);

  if (result != CURLE_OK) {
    LOG_FATAL << "Error connecting to TUF repo: " << result << ": " << curl_easy_strerror(result);
    return EXIT_FAILURE;
  }

  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code != 200) {
    LOG_FATAL << "Error " << http_code << " getting targets.json from TUF repo: " << targets_str;
  }

  Json::Value targets_json = Utils::parseJSON(targets_str);
  Json::Value target_list = targets_json["signed"]["targets"];
  for (Json::ValueIterator t_it = target_list.begin(); t_it != target_list.end(); t_it++) {
    if ((*t_it)["hashes"]["sha256"].asString() == ref) {
      LOG_INFO << "OSTree package " << ref << " is found in targets.json";
      return EXIT_SUCCESS;
    }
  }
  LOG_INFO << "OSTree package " << ref << " was not found in targets.json";
  return EXIT_FAILURE;
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
