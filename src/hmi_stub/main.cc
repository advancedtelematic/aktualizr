#include <iostream>
#include <thread>

#include <openssl/ssl.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/signals2.hpp>

#include "config/config.h"
#include "logging/logging.h"
#include "primary/aktualizr.h"
#include "utilities/utils.h"

namespace bpo = boost::program_options;

bool shutting_down{false};
unsigned int progress = 0;
std::vector<Uptane::Target> updates;

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    std::cout << "Available commands: Shutdown, SendDeviceData, FetchMeta, StartDownload, UptaneInstall\n";
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

void process_event(const std::shared_ptr<event::BaseEvent> &event) {
  if (event->variant == "DownloadProgressReport") {
    unsigned int new_progress = dynamic_cast<event::DownloadProgressReport *>(event.get())->progress;
    if (new_progress > progress) {
      progress = new_progress;
      std::cout << "Download progress: " << progress << "%\n";
    }
    return;
  }
  std::cout << "Received " << event->variant << " event\n";
  if (event->variant == "DownloadComplete") {
    progress = 0;
  } else if (event->variant == "UpdateAvailable") {
    updates = dynamic_cast<event::UpdateAvailable *>(event.get())->updates;
  } else if (event->variant == "InstallComplete") {
    updates.clear();
  }
}

void get_user_input(const std::shared_ptr<Aktualizr> &aktualizr) {
  std::string buffer;
  while (!shutting_down && std::getline(std::cin, buffer)) {
    if (buffer == "Shutdown") {
      aktualizr->sendCommand(std::make_shared<command::Shutdown>());
      return;
    } else if (buffer == "SendDeviceData") {
      aktualizr->sendCommand(std::make_shared<command::SendDeviceData>());
    } else if (buffer == "FetchMeta") {
      aktualizr->sendCommand(std::make_shared<command::FetchMeta>());
    } else if (buffer == "StartDownload") {
      aktualizr->sendCommand(std::make_shared<command::StartDownload>(updates));
    } else if (buffer == "UptaneInstall") {
      aktualizr->sendCommand(std::make_shared<command::UptaneInstall>(updates));
    } else if (!buffer.empty()) {
      std::cout << "Unknown command.\n";
    }
  }
}

int main(int argc, char *argv[]) {
  logger_init();
  logger_set_threshold(boost::log::trivial::info);
  LOG_INFO << "hmi_stub version " AKTUALIZR_VERSION " starting";

  bpo::variables_map commandline_map = parse_options(argc, argv);

  int r = -1;
  boost::signals2::connection conn;
  std::thread ui_thread;

  try {
    Config config(commandline_map);
    config.uptane.running_mode = RunningMode::kManual;
    if (config.logger.loglevel <= boost::log::trivial::debug) {
      SSL_load_error_strings();
    }
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();
    std::shared_ptr<Aktualizr> aktualizr = std::make_shared<Aktualizr>(config);

    std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_event;
    conn = aktualizr->setSignalHandler(f_cb);

    ui_thread = std::thread(get_user_input, aktualizr);

    r = aktualizr->run();
  } catch (const std::exception &ex) {
    LOG_ERROR << ex.what();
  }

  shutting_down = true;
  conn.disconnect();
  if (ui_thread.joinable()) {
    ui_thread.join();
  }
  return r;
}
