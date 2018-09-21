#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <openssl/ssl.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/signals2.hpp>

#include "config/config.h"
#include "logging/logging.h"
#include "primary/aktualizr.h"
#include "uptane/secondaryfactory.h"
#include "utilities/utils.h"

namespace bpo = boost::program_options;

unsigned int progress = 0;
std::vector<Uptane::Target> updates;
std::mutex pending_ecus_mutex;
unsigned int pending_ecus;

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    std::cout << "Available commands: Shutdown, SendDeviceData, FetchMetadata, Download, Install, CampaignCheck\n";
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current hmi_stub version is: " << AKTUALIZR_VERSION << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char *argv[]) {
  bpo::options_description description("HMI stub interface for libaktualizr");
  // clang-format off
  // Try to keep these options in the same order as Config::updateFromCommandLine().
  // The first three are commandline only.
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr version")
      ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory")
      ("secondary", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "secondary ECU json configuration file")
      ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)");
  // clang-format on

  bpo::variables_map vm;
  std::vector<std::string> unregistered_options;
  try {
    bpo::basic_parsed_options<char> parsed_options =
        bpo::command_line_parser(argc, argv).options(description).allow_unregistered().run();
    bpo::store(parsed_options, vm);
    check_info_options(description, vm);
    bpo::notify(vm);
    unregistered_options = bpo::collect_unrecognized(parsed_options.options, bpo::include_positional);
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

void process_event(Aktualizr &aktualizr, const std::shared_ptr<event::BaseEvent> &event) {
  if (event->variant == "DownloadProgressReport") {
    unsigned int new_progress = dynamic_cast<event::DownloadProgressReport *>(event.get())->progress;
    if (new_progress > progress) {
      progress = new_progress;
      std::cout << "Download progress: " << progress << "%\n";
    }
    return;
  }
  if (event->variant == "FetchMetaComplete") {
    aktualizr.CheckUpdates();
  } else if (event->variant == "AllDownloadsComplete") {
    std::cout << "Received " << event->variant << " event\n";
    progress = 0;
  } else if (event->variant == "UpdateAvailable") {
    const auto updateAvailable = dynamic_cast<event::UpdateAvailable *>(event.get());
    updates = updateAvailable->updates;
    std::cout << updateAvailable->ecus_count << " updates available\n";
  } else if (event->variant == "InstallStarted") {
    const auto install_started = dynamic_cast<event::InstallStarted *>(event.get());
    std::cout << "Installation started for device " << install_started->serial.ToString() << "\n";
  } else if (event->variant == "InstallComplete") {
    const auto install_complete = dynamic_cast<event::InstallComplete *>(event.get());
    std::cout << "Installation complete for device " << install_complete->serial.ToString() << "\n";
  } else if (event->variant == "AllInstallsComplete") {
    updates.clear();
    std::cout << "All installations complete.\n";
  } else {
    std::cout << "Received " << event->variant << " event\n";
  }
}

int main(int argc, char *argv[]) {
  logger_init();
  logger_set_threshold(boost::log::trivial::info);
  LOG_INFO << "hmi_stub version " AKTUALIZR_VERSION " starting";

  bpo::variables_map commandline_map = parse_options(argc, argv);

  int r = -1;
  boost::signals2::connection conn;

  try {
    Config config(commandline_map);
    if (config.logger.loglevel <= boost::log::trivial::debug) {
      SSL_load_error_strings();
    }
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();

    Aktualizr aktualizr(config);
    std::function<void(const std::shared_ptr<event::BaseEvent> event)> f_cb =
        [&aktualizr](const std::shared_ptr<event::BaseEvent> event) { process_event(aktualizr, event); };
    conn = aktualizr.SetSignalHandler(f_cb);

    if (commandline_map.count("secondary") != 0) {
      auto sconfigs = commandline_map["secondary"].as<std::vector<boost::filesystem::path>>();
      for (const auto &sconf : sconfigs) {
        aktualizr.AddSecondary(Uptane::SecondaryFactory::makeSecondary(sconf));
      }
    }

    aktualizr.Initialize();

    std::string buffer;
    while (std::getline(std::cin, buffer)) {
      boost::algorithm::to_lower(buffer);
      if (buffer == "shutdown") {
        aktualizr.Shutdown();
        break;
      } else if (buffer == "senddevicedata") {
        aktualizr.SendDeviceData();
      } else if (buffer == "fetchmetadata" || buffer == "fetchmeta") {
        aktualizr.FetchMetadata();
      } else if (buffer == "download" || buffer == "startdownload") {
        aktualizr.Download(updates);
      } else if (buffer == "install" || buffer == "uptaneinstall") {
        aktualizr.Install(updates);
      } else if (buffer == "campaigncheck") {
        aktualizr.CampaignCheck();
      } else if (!buffer.empty()) {
        std::cout << "Unknown command.\n";
      }
    }
    r = 0;
  } catch (const std::exception &ex) {
    LOG_ERROR << "Fatal error in hmi_stub: " << ex.what();
  }

  conn.disconnect();
  return r;
}
