#include <string>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "accumulator.h"
#include "authenticate.h"
#include "deploy.h"
#include "garage_common.h"
#include "garage_tools_version.h"
#include "logging/logging.h"
#include "ostree_dir_repo.h"
#include "ostree_repo.h"
#include "utilities/xml2json.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  logger_init();

  int verbosity;
  boost::filesystem::path repo_path;
  std::string ref;
  boost::filesystem::path credentials_path;
  std::string cacerts;
  boost::filesystem::path manifest_path;
  int max_curl_requests;
  RunMode mode = RunMode::kDefault;
  po::options_description desc("garage-push command line options");
  // clang-format off
  desc.add_options()
    ("help", "print usage")
    ("version", "Current garage-push version")
    ("verbose,v", accumulator<int>(&verbosity), "Verbose logging (use twice for more information)")
    ("quiet,q", "Quiet mode")
    ("repo,C", po::value<boost::filesystem::path>(&repo_path)->required(), "location of OSTree repo")
    ("ref,r", po::value<std::string>(&ref)->required(), "OSTree ref to push (or commit refhash)")
    ("credentials,j", po::value<boost::filesystem::path>(&credentials_path)->required(), "credentials (json or zip containing json)")
    ("cacert", po::value<std::string>(&cacerts), "override path to CA root certificates, in the same format as curl --cacert")
    ("repo-manifest", po::value<boost::filesystem::path>(&manifest_path), "manifest describing repository branches used in the image, to be sent as attached metadata")
    ("jobs", po::value<int>(&max_curl_requests)->default_value(30), "maximum number of parallel requests")
    ("dry-run,n", "check arguments and authenticate but don't upload")
    ("walk-tree,w", "walk entire tree and upload all missing objects");
  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, reinterpret_cast<const char *const *>(argv), desc), vm);

    if (vm.count("help") != 0U) {
      LOG_INFO << desc;
      return EXIT_SUCCESS;
    }
    if (vm.count("version") != 0) {
      LOG_INFO << "Current garage-push version is: " << garage_tools_version();
      exit(EXIT_SUCCESS);
    }
    po::notify(vm);
  } catch (const po::error &o) {
    LOG_ERROR << o.what();
    LOG_ERROR << desc;
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

  Utils::setUserAgent(std::string("garage-push/") + garage_tools_version());

  if (cacerts != "") {
    if (!boost::filesystem::exists(cacerts)) {
      LOG_FATAL << "--cacert path " << cacerts << " does not exist";
      return EXIT_FAILURE;
    }
  }

  if (vm.count("dry-run") != 0U) {
    mode = RunMode::kDryRun;
  }
  if (vm.count("walk-tree") != 0U) {
    // If --walk-tree and --dry-run were provided, walk but do not push.
    if (mode == RunMode::kDryRun) {
      mode = RunMode::kWalkTree;
    } else {
      mode = RunMode::kPushTree;
    }
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
    std::unique_ptr<OSTreeHash> commit;
    bool is_ref = true;
    OSTreeRef ostree_ref = src_repo->GetRef(ref);
    if (ostree_ref.IsValid()) {
      commit = std_::make_unique<OSTreeHash>(ostree_ref.GetHash());
    } else {
      try {
        commit = std_::make_unique<OSTreeHash>(OSTreeHash::Parse(ref));
      } catch (const OSTreeCommitParseError &e) {
        LOG_FATAL << "Ref or commit refhash " << ref << " was not found in repository " << repo_path.string();
        return EXIT_FAILURE;
      }
      is_ref = false;
    }

    ServerCredentials push_credentials(credentials_path);
    TreehubServer push_server;
    if (authenticate(cacerts, push_credentials, push_server) != EXIT_SUCCESS) {
      LOG_FATAL << "Authentication with push server failed";
      return EXIT_FAILURE;
    }
    if (!UploadToTreehub(src_repo, push_server, *commit, mode, max_curl_requests)) {
      LOG_FATAL << "Upload to treehub failed";
      return EXIT_FAILURE;
    }

    if (mode != RunMode::kDryRun) {
      if (is_ref) {
        if (!PushRootRef(push_server, ostree_ref)) {
          LOG_FATAL << "Error pushing root ref to treehub";
          return EXIT_FAILURE;
        }
      } else {
        LOG_INFO << "Provided ref " << ref << " is a commit refhash. Cannot push root ref";
      }
    }

    if (manifest_path != "") {
      try {
        std::string manifest_json_str;
        std::ifstream ifs(manifest_path.string());
        std::stringstream ss;
        auto manifest_json = xml2json::xml2json(ifs);
        ss << manifest_json;
        manifest_json_str = ss.str();

        LOG_INFO << "Sending manifest:\n" << manifest_json_str;
        if (mode != RunMode::kDryRun) {
          CURL *curl = curl_easy_init();
          curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
          push_server.SetContentType("Content-Type: application/json");
          push_server.InjectIntoCurl("manifests/" + commit->string(), curl);
          curlEasySetoptWrapper(curl, CURLOPT_CUSTOMREQUEST, "PUT");
          curlEasySetoptWrapper(curl, CURLOPT_POSTFIELDS, manifest_json_str.c_str());
          CURLcode rc = curl_easy_perform(curl);

          if (rc != CURLE_OK) {
            LOG_ERROR << "Error pushing repo manifest to Treehub";
          }
          curl_easy_cleanup(curl);
        }
      } catch (std::exception &e) {
        LOG_ERROR << "Could not send repo manifest to Treehub";
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
