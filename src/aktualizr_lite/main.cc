#include <unistd.h>
#include <iostream>

#include <openssl/ssl.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "config/config.h"
#include "package_manager/ostreemanager.h"
#include "primary/sotauptaneclient.h"

namespace bpo = boost::program_options;

static int status_main(Config &config, const bpo::variables_map &unused) {
  (void)unused;
  GObjectUniquePtr<OstreeSysroot> sysroot_smart = OstreeManager::LoadSysroot(config.pacman.sysroot);
  OstreeDeployment *deployment = ostree_sysroot_get_booted_deployment(sysroot_smart.get());
  if (deployment == nullptr) {
    LOG_INFO << "No active deployment found";
  } else {
    LOG_INFO << "Active image is: " << ostree_deployment_get_csum(deployment);
  }
  return 0;
}

static int list_main(Config &config, const bpo::variables_map &unused) {
  (void)unused;
  auto storage = INvStorage::newStorage(config.storage);
  auto client = SotaUptaneClient::newDefaultClient(config, storage);
  Uptane::HardwareIdentifier hwid(config.provision.primary_ecu_hardware_id);

  LOG_INFO << "Refreshing target metadata";
  if (!client->updateImagesMeta()) {
    LOG_WARNING << "Unable to update latest metadata, using local copy";
    if (!client->checkImagesMetaOffline()) {
      LOG_ERROR << "Unable to use local copy of TUF data";
      return 1;
    }
  }

  LOG_INFO << "Updates available to " << hwid << ":";
  for (auto &t : client->allTargets()) {
    for (auto const &it : t.hardwareIds()) {
      if (it == hwid) {
        auto name = t.filename();
        if (t.custom_version().length() > 0) {
          name = t.custom_version();
        }
        LOG_INFO << name << "\tsha256:" << t.sha256Hash();
        if (config.pacman.type == PackageManager::kOstreeDockerApp) {
          bool shown = false;
          auto apps = t.custom_data()["docker_apps"];
          for (Json::ValueIterator i = apps.begin(); i != apps.end(); ++i) {
            if (!shown) {
              shown = true;
              LOG_INFO << "\tDocker Apps:";
            }
            if ((*i).isObject() && (*i).isMember("filename")) {
              LOG_INFO << "\t\t" << i.key().asString() << " -> " << (*i)["filename"].asString();
            } else {
              LOG_ERROR << "\t\tInvalid custom data for docker-app: " << i.key().asString();
            }
          }
        }
        break;
      }
    }
  }
  return 0;
}

static std::unique_ptr<Uptane::Target> find_target(const std::shared_ptr<SotaUptaneClient> &client,
                                                   Uptane::HardwareIdentifier &hwid, const std::string &version) {
  std::unique_ptr<Uptane::Target> rv;
  if (!client->updateImagesMeta()) {
    LOG_WARNING << "Unable to update latest metadata, using local copy";
    if (!client->checkImagesMetaOffline()) {
      LOG_ERROR << "Unable to use local copy of TUF data";
      throw std::runtime_error("Unable to find update");
    }
  }
  for (auto &t : client->allTargets()) {
    for (auto const &it : t.hardwareIds()) {
      if (it == hwid) {
        if (version == "latest" || version == t.filename() || version == t.custom_version()) {
          return std_::make_unique<Uptane::Target>(t);
        }
      }
    }
  }
  throw std::runtime_error("Unable to find update");
}

static int update_main(Config &config, const bpo::variables_map &variables_map) {
  auto storage = INvStorage::newStorage(config.storage);
  auto client = SotaUptaneClient::newDefaultClient(config, storage);
  Uptane::HardwareIdentifier hwid(config.provision.primary_ecu_hardware_id);

  std::string version("latest");
  if (variables_map.count("update-name") > 0) {
    version = variables_map["update-name"].as<std::string>();
  }
  LOG_INFO << "Finding " << version << " to update to...";
  auto target = find_target(client, hwid, version);
  LOG_INFO << "Updating to: " << *target;

  std::vector<Uptane::Target> targets{*target};
  auto result = client->downloadImages(targets);
  if (result.status != result::DownloadStatus::kSuccess &&
      result.status != result::DownloadStatus::kNothingToDownload) {
    LOG_ERROR << "Unable to download update: " + result.message;
    return 1;
  }
  auto iresult = client->PackageInstall(*target);
  if (iresult.result_code.num_code == data::ResultCode::Numeric::kNeedCompletion) {
    LOG_INFO << "Update complete. Please reboot the device to activate";
  } else if (iresult.result_code.num_code != data::ResultCode::Numeric::kOk &&
             iresult.result_code.num_code != data::ResultCode::Numeric::kNeedCompletion) {
    LOG_ERROR << "Unable to install update: " << iresult.description;
    return 1;
  }
  LOG_INFO << iresult.description;
  return 0;
}

struct SubCommand {
  const char *name;
  int (*main)(Config &, const bpo::variables_map &);
};
static SubCommand commands[] = {
    {"status", status_main},
    {"list", list_main},
    {"update", update_main},
};

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0 || vm.count("command") == 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr version is: " << AKTUALIZR_VERSION << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char *argv[]) {
  std::string subs("Command to execute: ");
  for (size_t i = 0; i < sizeof(commands) / sizeof(SubCommand); i++) {
    if (i != 0) {
      subs += ", ";
    }
    subs += commands[i].name;
  }
  bpo::options_description description("aktualizr-lite command line options");
  // clang-format off
  // Try to keep these options in the same order as Config::updateFromCommandLine().
  // The first three are commandline only.
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr version")
      ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory")
      ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)")
      ("repo-server", bpo::value<std::string>(), "url of the uptane repo repository")
      ("ostree-server", bpo::value<std::string>(), "url of the ostree repository")
      ("primary-ecu-hardware-id", bpo::value<std::string>(), "hardware ID of primary ecu")
      ("update-name", bpo::value<std::string>(), "optional name of the update when running \"update\". default=latest")
      ("command", bpo::value<std::string>(), subs.c_str());
  // clang-format on

  // consider the first positional argument as the aktualizr run mode
  bpo::positional_options_description pos;
  pos.add("command", 1);

  bpo::variables_map vm;
  std::vector<std::string> unregistered_options;
  try {
    bpo::basic_parsed_options<char> parsed_options =
        bpo::command_line_parser(argc, argv).options(description).positional(pos).allow_unregistered().run();
    bpo::store(parsed_options, vm);
    check_info_options(description, vm);
    bpo::notify(vm);
    unregistered_options = bpo::collect_unrecognized(parsed_options.options, bpo::exclude_positional);
    if (vm.count("help") == 0 && !unregistered_options.empty()) {
      std::cout << description << "\n";
      exit(EXIT_FAILURE);
    }
  } catch (const bpo::required_option &ex) {
    // print the error and append the default commandline option description
    std::cout << ex.what() << std::endl << description;
    exit(EXIT_FAILURE);
  } catch (const bpo::error &ex) {
    check_info_options(description, vm);

    // log boost error
    LOG_ERROR << "boost command line option error: " << ex.what();

    // print the error message to the standard output too, as the user provided
    // a non-supported commandline option
    std::cout << ex.what() << '\n';

    // set the returnValue, thereby ctest will recognize
    // that something went wrong
    exit(EXIT_FAILURE);
  }

  return vm;
}

int main(int argc, char *argv[]) {
  logger_init(isatty(1) == 1);
  logger_set_threshold(boost::log::trivial::info);

  bpo::variables_map commandline_map = parse_options(argc, argv);

  int r = EXIT_FAILURE;
  try {
    if (geteuid() != 0) {
      LOG_WARNING << "\033[31mRunning as non-root and may not work as expected!\033[0m\n";
    }

    Config config(commandline_map);
    config.storage.uptane_metadata_path = BasedPath(config.storage.path / "metadata");
    if (config.logger.loglevel <= boost::log::trivial::debug) {
      SSL_load_error_strings();
    }
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();

    std::string cmd = commandline_map["command"].as<std::string>();
    for (size_t i = 0; i < sizeof(commands) / sizeof(SubCommand); i++) {
      if (cmd == commands[i].name) {
        return commands[i].main(config, commandline_map);
      }
    }
    throw bpo::invalid_option_value(cmd);
    r = EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    LOG_ERROR << ex.what();
  }
  return r;
}
