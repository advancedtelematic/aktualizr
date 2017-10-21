
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>

#include "accumulator.h"
#include "deploy.h"
#include "logging.h"

namespace po = boost::program_options;
using std::string;

int main(int argc, char **argv) {
  logger_init();

  string repo_path;
  string ref;
  string pull_cred;
  int maxCurlRequests;

  string credentials_path;
  string home_path = string(getenv("HOME"));
  string cacerts;

  int verbosity;
  bool dry_run = false;
  po::options_description desc("Allowed options");
  // clang-format off
  desc.add_options()
    ("help", "produce a help message")
    ("verbose,v", accumulator<int>(&verbosity), "Verbose logging (use twice for more information)")
    ("quiet,q", "Quiet mode")
    ("repo,C", po::value<string>(&repo_path), "location of ostree repo")
    ("ref,r", po::value<string>(&ref)->required(), "ref to push")
    ("fetch-credentials,f", po::value<string>(&pull_cred), "path to credentials to fetch from")
    ("credentials,j", po::value<string>(&credentials_path)->default_value(home_path + "/.sota_tools.json"), "Credentials (json or zip containing json)")
    ("cacert", po::value<string>(&cacerts), "Override path to CA root certificates, in the same format as curl --cacert")
    ("jobs", po::value<int>(&maxCurlRequests)->default_value(30), "Maximum number of parallel requests")
    ("dry-run,n", "Dry Run: Check arguments and authenticate but don't upload");
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

  if (!vm.count("repo") && !vm.count("fetch-credentials")) {
    LOG_INFO << "You should specify --repo or --fetch-credentials";
    return EXIT_FAILURE;
  }

  if (vm.count("repo") && vm.count("fetch-credentials")) {
    LOG_INFO << "You cannot specify --repo and --fetch-credentials options together";
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

  if (cacerts != "") {
    if (!boost::filesystem::exists(cacerts)) {
      LOG_FATAL << "--cacert path " << cacerts << " does not exist";
      return EXIT_FAILURE;
    }
  }

  if (vm.count("dry-run")) {
    dry_run = true;
  }

  std::string src = repo_path;
  if (!pull_cred.empty()) {
    src = pull_cred;
  }
  return copy_repo(cacerts, src, credentials_path, ref, dry_run);
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
