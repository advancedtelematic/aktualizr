/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file main.cpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  The main file of the project.
 *
 *
 ******************************************************************************
 *
 * \endcond
 */

/*****************************************************************************/
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>

#include "aktualizr.h"
#include "config.h"
#include "logger.h"
#include "utils.h"

/*****************************************************************************/

namespace bpo = boost::program_options;

bpo::variables_map parse_options(int argc, char *argv[]) {
  bpo::options_description description("CommandLine Options");
  // clang-format off
  description.add_options()
      ("help,h", "help screen")
      ("version,v", "Current aktualizr version")
      ("loglevel", bpo::value<int>(), "set log level 0-4 (trace, debug, warning, info, error)")
      ("config,c", bpo::value<std::string>()->required(), "toml configuration file")
      ("gateway-http", bpo::value<bool>(), "enable the http gateway")
      ("gateway-rvi", bpo::value<bool>(), "enable the rvi gateway")
      ("gateway-socket", bpo::value<bool>(), "enable the socket gateway")
      ("gateway-dbus", bpo::value<bool>(), "enable the D-Bus gateway")
      ("dbus-system-bus", "Use the D-Bus system bus (rather than the session bus)")
      ("tls-server", bpo::value<std::string>(), "url, used for auto provisioning")
      ("repo-server", bpo::value<std::string>(), "url of the uptane repo repository")
      ("director-server", bpo::value<std::string>(), "url of the uptane director repository")
      ("ostree-server", bpo::value<std::string>(), "url of the ostree repository")
      ("primary-ecu-serial", bpo::value<std::string>(), "serial number of primary ecu")
      ("primary-ecu-hardware-id", bpo::value<std::string>(), "hardware id of primary ecu")
      ("disable-keyid-validation", "Disable keyid validation on client side" )
      ("poll-once", "Check for updates only once and exit")
      ("secondary-config", bpo::value<std::vector<std::string> >()->composing(), "set config for secondary");
  // clang-format on

  bpo::variables_map vm;
  try {
    bpo::store(bpo::parse_command_line(argc, argv, description), vm);
    bpo::notify(vm);
  } catch (const bpo::required_option &ex) {
    if (vm.count("help") != 0) {
      std::cout << description << '\n';
      exit(EXIT_SUCCESS);
    }
    if (vm.count("version") != 0) {
      std::cout << "Current aktualizr version is: " << AKTUALIZR_VERSION << "\n";
      exit(EXIT_SUCCESS);
    }

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
    // log boost error
    LOGGER_LOG(LVL_warning, "boost command line option error: " << ex.what());

    // print the error message to the standard output too, as the user provided
    // a non-supported commandline option
    std::cout << ex.what() << '\n';

    // set the returnValue, thereby ctest will recognize
    // that something went wrong
    exit(EXIT_FAILURE);
  }

  return vm;
}

/*****************************************************************************/
int main(int argc, char *argv[]) {
  loggerInit();

  bpo::variables_map commandline_map = parse_options(argc, argv);

  // check for loglevel
  if (commandline_map.count("loglevel") != 0) {
    // set the log level from command line option
    LoggerLevels severity = static_cast<LoggerLevels>(commandline_map["loglevel"].as<int>());
    if (severity < LVL_minimum) {
      LOGGER_LOG(LVL_debug, "Invalid log level");
      severity = LVL_trace;
    }
    if (LVL_maximum < severity) {
      LOGGER_LOG(LVL_warning, "Invalid log level");
      severity = LVL_error;
    }
    loggerSetSeverity(severity);
  }

  // Initialize config with default values, the update with config, then with cmd
  try {
    std::string filename = commandline_map["config"].as<std::string>();
    Config config(filename, commandline_map);
    Aktualizr aktualizr(config);
    return aktualizr.run();
  } catch(std::runtime_error e) {
    LOGGER_LOG(LVL_error, "ERROR: " << e.what());
    exit(EXIT_FAILURE);
  }
}
