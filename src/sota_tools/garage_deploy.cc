#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "accumulator.h"
#include "deploy.h"
#include "logging.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  logger_init();
  int verbosity;
  std::string ref;
  std::string fetch_cred;
  std::string push_cred;
  std::string hardwareids;
  std::string cacerts;
  po::options_description desc("garage_push command line options");
  // clang-format off
  desc.add_options()
    ("help", "print usage")
    ("version,v", "Current garage-deploy version")
    ("verbose,l", accumulator<int>(&verbosity), "verbose logging (use twice for more information)")
    ("ref,r", po::value<std::string>(&ref)->required(), "package name to fetch")
    ("fetch-credentials,f", po::value<std::string>(&fetch_cred)->required(), "path to fetch credentials file")
    ("push-credentials,p", po::value<std::string>(&push_cred)->required(), "path to push credentials file")
    ("hardwareids,h", po::value<std::string>(&hardwareids)->required(), "list of hardware ids")
    ("cacert", po::value<std::string>(&cacerts), "override path to CA root certificates, in the same format as curl --cacert");

  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, (const char *const *)argv, desc), vm);

    if (vm.count("help")) {
      LOG_INFO << desc;
      return EXIT_SUCCESS;
    }
    if (vm.count("version") != 0) {
      LOG_INFO << "Current garage-deploy version is: " << GARAGE_DEPLOY_VERSION << "\n";
      exit(EXIT_SUCCESS);
    }
    po::notify(vm);
  } catch (const po::error &o) {
    LOG_ERROR << o.what();
    LOG_ERROR << desc;
    return EXIT_FAILURE;
  }

  if (!vm.count("ref") && !vm.count("fetch-credentials") && vm.count("push-credentials")) {
    LOG_INFO << "You must specify crdentials for source repo, destination repo, and package name";
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

  return !copy_repo(cacerts, fetch_cred, push_cred, ref, hardwareids, true);
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
