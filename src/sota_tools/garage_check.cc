#include <string>

#include <curl/curl.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "authenticate.h"
#include "check.h"
#include "garage_common.h"
#include "garage_tools_version.h"
#include "logging/logging.h"
#include "ostree_http_repo.h"
#include "ostree_object.h"
#include "request_pool.h"
#include "treehub_server.h"
#include "utilities/types.h"
#include "utilities/utils.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  logger_init();

  std::string ref;
  boost::filesystem::path credentials_path;
  std::string cacerts;
  int max_curl_requests;
  RunMode mode = RunMode::kDefault;
  boost::filesystem::path tree_dir;
  po::options_description desc("garage-check command line options");
  // clang-format off
  desc.add_options()
    ("help", "print usage")
    ("version", "Current garage-check version")
    ("verbose,v", "Verbose logging (loglevel 1)")
    ("quiet,q", "Quiet mode (loglevel 3)")
    ("loglevel", po::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)")
    ("ref,r", po::value<std::string>(&ref)->required(), "refhash to check")
    ("credentials,j", po::value<boost::filesystem::path>(&credentials_path)->required(), "credentials (json or zip containing json)")
    ("cacert", po::value<std::string>(&cacerts), "override path to CA root certificates, in the same format as curl --cacert")
    ("jobs", po::value<int>(&max_curl_requests)->default_value(30), "maximum number of parallel requests (only relevant with --walk-tree)")
    ("walk-tree,w", "walk entire tree and check presence of all objects")
    ("tree-dir,t", po::value<boost::filesystem::path>(&tree_dir), "directory to which to write the tree (only used with --walk-tree)");
  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, reinterpret_cast<const char *const *>(argv), desc), vm);

    if (vm.count("help") != 0U) {
      LOG_INFO << desc;
      return EXIT_SUCCESS;
    }
    if (vm.count("version") != 0) {
      LOG_INFO << "Current garage-check version is: " << garage_tools_version();
      exit(EXIT_SUCCESS);
    }
    po::notify(vm);
  } catch (const po::error &o) {
    LOG_ERROR << o.what();
    LOG_ERROR << desc;
    return EXIT_FAILURE;
  }

  try {
    // Configure logging. Try loglevel first, then verbose, then quiet.
    if (vm.count("loglevel") != 0) {
      const int loglevel = vm["loglevel"].as<int>();
      logger_set_threshold(static_cast<boost::log::trivial::severity_level>(loglevel));
      LOG_INFO << "Loglevel set to " << loglevel;
    } else if (static_cast<int>(vm.count("verbose")) != 0) {
      logger_set_threshold(boost::log::trivial::debug);
      LOG_DEBUG << "Debug level debugging enabled";
    } else if (static_cast<int>(vm.count("quiet")) != 0) {
      logger_set_threshold(boost::log::trivial::warning);
    } else {
      logger_set_threshold(boost::log::trivial::info);
    }

    Utils::setUserAgent(std::string("garage-check/") + garage_tools_version());

    if (vm.count("walk-tree") != 0U) {
      mode = RunMode::kWalkTree;
    }

    if (max_curl_requests < 1) {
      LOG_FATAL << "--jobs must be greater than 0";
      return EXIT_FAILURE;
    }

    TreehubServer treehub;
    if (authenticate(cacerts, ServerCredentials(credentials_path), treehub) != EXIT_SUCCESS) {
      LOG_FATAL << "Authentication failed";
      return EXIT_FAILURE;
    }

    if (CheckRefValid(treehub, ref, mode, max_curl_requests, tree_dir) != EXIT_SUCCESS) {
      LOG_FATAL << "Check if the ref is present on the server or in targets.json failed";
      return EXIT_FAILURE;
    }
  } catch (std::exception &ex) {
    LOG_ERROR << "Exception: " << ex.what();
    return EXIT_FAILURE;
  } catch (...) {
    LOG_ERROR << "Unknown exception";
    return EXIT_FAILURE;
  }
}
