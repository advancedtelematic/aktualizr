#include <iostream>

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

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr version is: " << AKTUALIZR_VERSION << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char *argv[]) {
  bpo::options_description description("aktualizr command line options");
  // clang-format off
  // Try to keep these options in the same order as Config::updateFromCommandLine().
  // The first three are commandline only.
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr version")
      ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory")
      ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)")
      ("run-mode", bpo::value<std::string>(), "run mode of aktualizr: full, once, campaign_check, campaign_accept, check, download, or install")
      ("tls-server", bpo::value<std::string>(), "url, used for auto provisioning")
      ("repo-server", bpo::value<std::string>(), "url of the uptane repo repository")
      ("director-server", bpo::value<std::string>(), "url of the uptane director repository")
      ("ostree-server", bpo::value<std::string>(), "url of the ostree repository")
      ("primary-ecu-serial", bpo::value<std::string>(), "serial number of primary ecu")
      ("primary-ecu-hardware-id", bpo::value<std::string>(), "hardware ID of primary ecu")
      ("secondary-configs-dir", bpo::value<boost::filesystem::path>(), "directory containing secondary ECU configuration files")
      ("campaign-id", bpo::value<std::string>(), "id of the campaign to act on");
  // clang-format on

  // consider the first positional argument as the aktualizr run mode
  bpo::positional_options_description pos;
  pos.add("run-mode", 1);

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

void process_event(const std::shared_ptr<event::BaseEvent> &event) {
  if (event->isTypeOf(event::DownloadProgressReport::TypeName)) {
    // Do nothing; libaktualizr already logs it.
  } else if (event->variant == "UpdateCheckComplete") {
    // Do nothing; libaktualizr already logs it.
  } else {
    LOG_INFO << "got " << event->variant << " event";
  }
}

int main(int argc, char *argv[]) {
  logger_init();
  logger_set_threshold(boost::log::trivial::info);

  bpo::variables_map commandline_map = parse_options(argc, argv);

  LOG_INFO << "Aktualizr version " AKTUALIZR_VERSION " starting";

  int r = EXIT_FAILURE;

  try {
    if (geteuid() != 0) {
      LOG_WARNING << "\033[31mAktualizr is currently running as non-root and may not work as expected! Aktualizr "
                     "should be run as root for proper functionality.\033[0m\n";
    }
    Config config(commandline_map);
    if (config.logger.loglevel <= boost::log::trivial::debug) {
      SSL_load_error_strings();
    }
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();

    Aktualizr aktualizr(config);
    std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_event;
    boost::signals2::scoped_connection conn;

    conn = aktualizr.SetSignalHandler(f_cb);
    aktualizr.Initialize();

    std::string run_mode;
    if (commandline_map.count("run-mode") != 0) {
      run_mode = commandline_map["run-mode"].as<std::string>();
    }
    // launch the first event
    if (run_mode == "campaign_check") {
      aktualizr.CampaignCheck().get();
    } else if (run_mode == "campaign_accept") {
      if (commandline_map.count("campaign-id") == 0) {
        throw std::runtime_error("Accepting a campaign requires a campaign ID");
      }
      aktualizr.CampaignAccept(commandline_map["campaign-id"].as<std::string>()).get();
    } else if (run_mode == "check") {
      aktualizr.SendDeviceData().get();
      aktualizr.CheckUpdates().get();
    } else if (run_mode == "download") {
      result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
      aktualizr.Download(update_result.updates).get();
    } else if (run_mode == "install") {
      result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
      aktualizr.Install(update_result.updates).get();
    } else if (run_mode == "once") {
      aktualizr.UptaneCycle();
    } else {
      aktualizr.RunForever().get();
    }
    r = EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    LOG_ERROR << ex.what();
  }

  return r;
}
