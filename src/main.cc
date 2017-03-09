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
#include <signal.h>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <memory>

#include "channel.h"
#include "commandinterpreter.h"
#include "commands.h"
#include "config.h"
#include "events.h"
#include "eventsinterpreter.h"
#include "logger.h"
#include "sotahttpclient.h"
#ifdef WITH_GENIVI
#include "sotarviclient.h"
#endif

/*****************************************************************************/

namespace bpo = boost::program_options;

bpo::variables_map parse_options(int argc, char *argv[]) {
  bpo::options_description description("CommandLine Options");
  description.add_options()("help,h", "help screen")("loglevel", bpo::value<int>(),
                                                     "set log level 0-4 (trace, debug, warning, info, error)")(
      "config,c", bpo::value<std::string>()->required(), "toml configuration file")(
      "gateway-http", bpo::value<bool>(), "enable the http gateway")("gateway-rvi", bpo::value<bool>(),
                                                                     "enable the rvi gateway")(
      "gateway-socket", bpo::value<bool>(), "enable the socket gateway")("gateway-dbus", bpo::value<bool>(),
                                                                         "enable the D-Bus gateway");

  bpo::variables_map vm;
  try {
    bpo::store(bpo::parse_command_line(argc, argv, description), vm);
    bpo::notify(vm);
  } catch (const bpo::required_option &ex) {
    if (ex.get_option_name() == "--config" && vm.count("help") == 0) {
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

  if (vm.count("help") != 0) {
    // print the description off all known command line options
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  return vm;
}

/*****************************************************************************/
int main(int argc, char *argv[]) {
  // create and initialize the return value to zero (everything is OK)
  int return_value = EXIT_SUCCESS;

  loggerInit();

  // Initialize config with default values
  Config config;

  bpo::variables_map commandline_map = parse_options(argc, argv);

  // check for config file commandline option
  // The config file has to be checked before checking for loglevel
  // as the loglevel option shall overwrite settings provided via
  // a configuration file.
  if (commandline_map.count("config") != 0) {
    std::string filename = commandline_map["config"].as<std::string>();
    try {
      config.updateFromToml(filename);
    } catch (boost::property_tree::ini_parser_error e) {
      LOGGER_LOG(LVL_error, "Exception was thrown while parsing " << filename
                                                                  << " config file, message: " << e.message());
      return EXIT_FAILURE;
    } catch (std::logic_error e){
      LOGGER_LOG(LVL_error, "Improperly configured error. "  << e.what());
      return EXIT_FAILURE;
    } 
  }

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

  config.updateFromCommandLine(commandline_map);

  command::Channel *commands_channel = new command::Channel();
  event::Channel *events_channel = new event::Channel();

  EventsInterpreter events_interpreter(config, events_channel, commands_channel);
  // run events interpreter in background
  events_interpreter.interpret();

  SotaClient *client;
  if (config.gateway.rvi) {
#ifdef WITH_GENIVI
    client = new SotaRVIClient(config, events_channel);
#else
    LOGGER_LOG(LVL_error, "RVI support is not enabled");
    return EXIT_FAILURE;
#endif
  } else {
    client = new SotaHttpClient(config, events_channel, commands_channel);
  }

  CommandInterpreter command_interpreter(client, commands_channel, events_channel);
  command_interpreter.interpret();

  return return_value;
}
