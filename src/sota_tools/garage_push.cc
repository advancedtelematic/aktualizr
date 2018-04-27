#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>

#include "accumulator.h"
#include "deploy.h"
#include "logging/logging.h"
#include "ostree_dir_repo.h"
#include "ostree_repo.h"

namespace po = boost::program_options;
using std::string;

int main(int argc, char **argv) {
  logger_init();

  boost::filesystem::path repo_path;
  string ref;
  int max_curl_requests;

  string home_path = string(getenv("HOME"));
  string cacerts;
  boost::filesystem::path credentials_path(home_path + "/.sota_tools.json");

  int verbosity;
  bool dry_run = false;
  po::options_description desc("garage-push command line options");
  // clang-format off
  desc.add_options()
    ("help", "print usage")
    ("verbose,v", accumulator<int>(&verbosity), "Verbose logging (use twice for more information)")
    ("quiet,q", "Quiet mode")
    ("repo,C", po::value<boost::filesystem::path>(&repo_path)->required(), "location of ostree repo")
    ("ref,r", po::value<string>(&ref)->required(), "ref to push")
    ("credentials,j", po::value<boost::filesystem::path>(&credentials_path), "credentials (json or zip containing json)")
    ("cacert", po::value<string>(&cacerts), "override path to CA root certificates, in the same format as curl --cacert")
    ("jobs", po::value<int>(&max_curl_requests)->default_value(30), "maximum number of parallel requests")
    ("dry-run,n", "check arguments and authenticate but don't upload");
  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, reinterpret_cast<const char *const *>(argv), desc), vm);

    if (vm.count("help") != 0u) {
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

  if (cacerts != "") {
    if (!boost::filesystem::exists(cacerts)) {
      LOG_FATAL << "--cacert path " << cacerts << " does not exist";
      return EXIT_FAILURE;
    }
  }

  if (vm.count("dry-run") != 0u) {
    dry_run = true;
  }

  if (max_curl_requests < 1) {
    LOG_FATAL << "--jobs must be greater than 0";
    return EXIT_FAILURE;
  }

  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeDirRepo>(repo_path);

  if (!src_repo->LooksValid()) {
    LOG_FATAL << "The OSTree src repository does not appear to contain a valid OSTree repository";
    return EXIT_FAILURE;
  }

  try {
    ServerCredentials push_credentials(credentials_path);
    OSTreeRef ostree_ref = src_repo->GetRef(ref);
    if (!ostree_ref.IsValid()) {
      LOG_FATAL << "Ref " << ref << " was not found in repository " << repo_path.string();
      return EXIT_FAILURE;
    }

    OSTreeHash commit(ostree_ref.GetHash());
    bool ok = UploadToTreehub(src_repo, push_credentials, commit, cacerts, dry_run, max_curl_requests);

    if (!ok) {
      LOG_FATAL << "Upload to treehub failed";
      return EXIT_FAILURE;
    }

    if (push_credentials.CanSignOffline()) {
      LOG_INFO << "Credentials contain offline signing keys, not pushing root ref";
    } else {
      ok = PushRootRef(push_credentials, ostree_ref, cacerts, dry_run);
      if (!ok) {
        LOG_FATAL << "Could not push root reference to treehub";
        return EXIT_FAILURE;
      }
    }
  } catch (const BadCredentialsArchive &e) {
    LOG_FATAL << e.what();
    return EXIT_FAILURE;
  } catch (const BadCredentialsContent &e) {
    LOG_FATAL << e.what();
    return EXIT_FAILURE;
  } catch (const BadCredentialsJson &e) {
    LOG_FATAL << e.what();
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
