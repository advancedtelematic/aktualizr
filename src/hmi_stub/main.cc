#include <iostream>
#include <map>
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

std::map<std::string, unsigned int> progress;
std::mutex pending_ecus_mutex;
unsigned int pending_ecus;

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    std::cout << "Available commands: Shutdown, SendDeviceData, CheckUpdates, Download, Install, CampaignCheck\n";
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
      ("secondary-configs-dir", bpo::value<boost::filesystem::path>(), "directory containing seconday ECU configuration files")
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

static std::vector<Uptane::Target> current_updates;

void process_event(const std::shared_ptr<event::BaseEvent> &event) {
  if (event->isTypeOf(event::DownloadProgressReport::TypeName)) {
    const auto download_progress = dynamic_cast<event::DownloadProgressReport *>(event.get());
    if (progress.find(download_progress->target.sha256Hash()) == progress.end()) {
      progress[download_progress->target.sha256Hash()] = 0;
    }
    const unsigned int prev_progress = progress[download_progress->target.sha256Hash()];
    const unsigned int new_progress = download_progress->progress;
    if (new_progress > prev_progress) {
      progress[download_progress->target.sha256Hash()] = new_progress;
      std::cout << "Download progress for file " << download_progress->target.filename() << ": " << new_progress
                << "%\n";
    }
  } else if (event->variant == "DownloadTargetComplete") {
    const auto download_complete = dynamic_cast<event::DownloadTargetComplete *>(event.get());
    std::cout << "Download complete for file " << download_complete->update.filename() << ": "
              << (download_complete->success ? "success" : "failure") << "\n";
    progress.erase(download_complete->update.sha256Hash());
  } else if (event->variant == "InstallStarted") {
    const auto install_started = dynamic_cast<event::InstallStarted *>(event.get());
    std::cout << "Installation started for device " << install_started->serial.ToString() << "\n";
  } else if (event->variant == "InstallTargetComplete") {
    const auto install_complete = dynamic_cast<event::InstallTargetComplete *>(event.get());
    std::cout << "Installation complete for device " << install_complete->serial.ToString() << ": "
              << (install_complete->success ? "success" : "failure") << "\n";
  } else if (event->variant == "UpdateCheckComplete") {
    const auto check_complete = dynamic_cast<event::UpdateCheckComplete *>(event.get());
    current_updates = check_complete->result.updates;
    std::cout << current_updates.size() << " updates available\n";
  } else {
    std::cout << "Received " << event->variant << " event\n";
  }
}

int main(int argc, char *argv[]) {
  logger_init();
  logger_set_threshold(boost::log::trivial::info);
  LOG_INFO << "hmi_stub version " AKTUALIZR_VERSION " starting";

  bpo::variables_map commandline_map = parse_options(argc, argv);

  int r = EXIT_FAILURE;
  boost::signals2::connection conn;

  try {
    Config config(commandline_map);
    if (config.logger.loglevel <= boost::log::trivial::debug) {
      SSL_load_error_strings();
    }
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();

    Aktualizr aktualizr(config);
    std::function<void(const std::shared_ptr<event::BaseEvent> event)> f_cb =
        [](const std::shared_ptr<event::BaseEvent> event) { process_event(event); };
    conn = aktualizr.SetSignalHandler(f_cb);

    aktualizr.Initialize();

    std::string buffer;
    while (std::getline(std::cin, buffer)) {
      boost::algorithm::to_lower(buffer);
      if (buffer == "shutdown") {
        break;
      } else if (buffer == "senddevicedata") {
        aktualizr.SendDeviceData();
      } else if (buffer == "fetchmetadata" || buffer == "fetchmeta" || buffer == "checkupdates" || buffer == "check") {
        auto fut_result = aktualizr.CheckUpdates();
      } else if (buffer == "download" || buffer == "startdownload") {
        aktualizr.Download(current_updates);
      } else if (buffer == "install" || buffer == "uptaneinstall") {
        aktualizr.Install(current_updates);
        current_updates.clear();
      } else if (buffer == "campaigncheck") {
        aktualizr.CampaignCheck();
      } else if (buffer == "pause") {
        aktualizr.Pause();
      } else if (buffer == "resume") {
        aktualizr.Resume();
      } else if (!buffer.empty()) {
        std::cout << "Unknown command.\n";
      }
    }
    r = EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    LOG_ERROR << "Fatal error in hmi_stub: " << ex.what();
  }

  conn.disconnect();
  return r;
}
