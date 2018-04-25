#include <iostream>

#include <openssl/ssl.h>
#include <sys/stat.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "aktualizr.h"
#include "config.h"
#include "logging.h"
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
      ("config,c", bpo::value<std::string>()->required(), "toml configuration file")
      ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, warning, info, error, fatal)")
      ("poll-once", "Check for updates only once and exit")
      ("gateway-socket", bpo::value<bool>(), "enable the socket gateway")
      ("tls-server", bpo::value<std::string>(), "url, used for auto provisioning")
      ("repo-server", bpo::value<std::string>(), "url of the uptane repo repository")
      ("director-server", bpo::value<std::string>(), "url of the uptane director repository")
      ("ostree-server", bpo::value<std::string>(), "url of the ostree repository")
      ("primary-ecu-serial", bpo::value<std::string>(), "serial number of primary ecu")
      ("primary-ecu-hardware-id", bpo::value<std::string>(), "hardware ID of primary ecu")
      ("secondary-config", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "secondary ECU json configuration file")
      ("legacy-interface", bpo::value<boost::filesystem::path>(), "path to legacy secondary ECU interface program")
      ("disable-keyid-validation", "deprecated");
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
    if (ex.get_option_name() == "--config") {
      std::cout << ex.get_option_name() << " is missing.\nYou have to provide a valid configuration "
                                           "file using toml format. See the example configuration file "
                                           "in config/config.toml.example"
                << std::endl;
      exit(EXIT_FAILURE);
    } else {
      // print the error and append the default commandline option description
      std::cout << ex.what() << std::endl << description;
      exit(EXIT_SUCCESS);
    }
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
  logger_init();
  logger_set_threshold(boost::log::trivial::info);
  LOG_INFO << "Aktualizr version " AKTUALIZR_VERSION " starting";

  bpo::variables_map commandline_map = parse_options(argc, argv);

  // Initialize config with default values, the update with config, then with cmd
  std::string sota_config_file = commandline_map["config"].as<std::string>();
  boost::filesystem::path sota_config_path(sota_config_file);
  if (!boost::filesystem::exists(sota_config_path)) {
    std::cerr << "aktualizr: configuration file " << boost::filesystem::absolute(sota_config_path)
              << " not found. Exiting." << std::endl;
    exit(EXIT_FAILURE);
  }

  try {
    if (geteuid() != 0) {
      LOG_WARNING << "\033[31mAktualizr is currently running as non-root and may not work as expected! Aktualizr "
                     "should be run as root for proper functionality.\033[0m\n";
    }

    Config config(sota_config_path, commandline_map);
    if (config.logger.loglevel <= boost::log::trivial::debug) {
      SSL_load_error_strings();
    }
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();

    boost::filesystem::path saved_config_path = "/tmp/aktualizr_config_path";
    Utils::writeFile(saved_config_path, boost::filesystem::absolute(sota_config_path).string());
    Aktualizr aktualizr(config);
    return aktualizr.run();
  } catch (const std::exception &ex) {
    LOG_ERROR << ex.what();
    return -1;
  }
}
