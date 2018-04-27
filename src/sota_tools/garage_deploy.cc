#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "accumulator.h"
#include "authenticate.h"
#include "deploy.h"
#include "logging/logging.h"
#include "ostree_http_repo.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  logger_init();
  int verbosity;
  std::string ostree_commit;
  std::string name;
  boost::filesystem::path fetch_cred;
  boost::filesystem::path push_cred;
  std::string hardwareids;
  std::string cacerts;
  po::options_description desc("garage-deploy command line options");
  // clang-format off
  desc.add_options()
    ("help", "print usage")
    ("version", "Current garage-deploy version")
    ("verbose,v", accumulator<int>(&verbosity), "verbose logging (use twice for more information)")
    ("quiet,q", "Quiet mode")
    ("commit", po::value<std::string>(&ostree_commit)->required(), "OSTree commit to deploy")
    ("name", po::value<std::string>(&name)->required(), "Name of image")
    ("fetch-credentials,f", po::value<boost::filesystem::path>(&fetch_cred)->required(), "path to source credentials")
    ("push-credentials,p", po::value<boost::filesystem::path>(&push_cred)->required(), "path to destination credentials")
    ("hardwareids,h", po::value<std::string>(&hardwareids)->required(), "list of hardware ids")
    ("cacert", po::value<std::string>(&cacerts), "override path to CA root certificates, in the same format as curl --cacert");
  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, reinterpret_cast<const char *const *>(argv), desc), vm);

    if (vm.count("help") != 0u) {
      LOG_INFO << desc;
      return EXIT_SUCCESS;
    }
    if (vm.count("version") != 0) {
      LOG_INFO << "Current garage-deploy version is: " << GARAGE_DEPLOY_VERSION;
      exit(EXIT_SUCCESS);
    }
    po::notify(vm);
  } catch (const po::error &o) {
    LOG_ERROR << o.what();
    LOG_ERROR << desc;
    return EXIT_FAILURE;
  }

  if ((vm.count("commit") == 0u) && (vm.count("fetch-credentials") == 0u) && (vm.count("push-credentials") != 0u)) {
    LOG_INFO << "You must specify credentials for source repo, destination repo, and package name";
    return EXIT_FAILURE;
  }

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

  ServerCredentials push_credentials(push_cred);
  ServerCredentials fetch_credentials(fetch_cred);

  TreehubServer fetch_server;
  if (authenticate(cacerts, fetch_credentials, fetch_server) != EXIT_SUCCESS) {
    LOG_FATAL << "Authentication failed";
    return EXIT_FAILURE;
  }
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&fetch_server);

  try {
    OSTreeHash commit(OSTreeHash::Parse(ostree_commit));
    // Since the fetches happen on a single thread in OSTreeHttpRepo, there
    // isn't really any reason to upload in parallel
    bool ok = UploadToTreehub(src_repo, push_credentials, commit, cacerts, false, 1);

    if (!ok) {
      LOG_FATAL << "Upload to treehub failed";
      return EXIT_FAILURE;
    }
    if (push_credentials.CanSignOffline()) {
      bool ok = OfflineSignRepo(ServerCredentials(push_credentials.GetPathOnDisk()), name, commit, hardwareids);
      return static_cast<int>(!ok);
    }
    LOG_FATAL << "Online signing with garage-deploy is currently unsupported";
    return EXIT_FAILURE;

  } catch (OSTreeCommitParseError &e) {
    LOG_FATAL << e.what();
    return EXIT_FAILURE;  // TODO: tests
  }
  return EXIT_SUCCESS;
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
